import cv2
import dlib
import pygame
import time
import requests
import threading
import numpy as np
import joblib
import os
import RPi.GPIO as GPIO
from flask import Flask, request, jsonify
from picamera2 import Picamera2
from threading import Lock
import datetime

# ========== KEYPAD SETUP ==========
GPIO.setmode(GPIO.BCM)
KEYPAD = [
    ["1", "2", "3", "A"],
    ["4", "5", "6", "B"],
    ["7", "8", "9", "C"],
    ["*", "0", "#", "D"]
]
ROW_PINS = [5, 6, 13, 19]
COL_PINS = [12, 16, 20, 21]

def setup_keypad():
    for row in ROW_PINS:
        GPIO.setup(row, GPIO.OUT)
        GPIO.output(row, GPIO.HIGH)
    for col in COL_PINS:
        GPIO.setup(col, GPIO.IN, pull_up_down=GPIO.PUD_UP)

def read_keypad():
    for row_index, row_pin in enumerate(ROW_PINS):
        GPIO.output(row_pin, GPIO.LOW)
        for col_index, col_pin in enumerate(COL_PINS):
            if GPIO.input(col_pin) == GPIO.LOW:
                time.sleep(0.02)
                while GPIO.input(col_pin) == GPIO.LOW:
                    time.sleep(0.01)
                GPIO.output(row_pin, GPIO.HIGH)
                return KEYPAD[row_index][col_index]
        GPIO.output(row_pin, GPIO.HIGH)
    return None

setup_keypad()
keypad_mode = None
keypad_buffer = ""
reminder_interval_alcohol = 300
reminder_max_drive_duration = 300
last_reminder_time_alcohol = time.time()
editing_index = -1
blink_state = True
last_blink_time = time.time()

# ========== FLASK SERVER ==========
app = Flask(__name__)
data_lock = Lock()
esp_data = {"heart_rate": "N/A", "gps_lat": "N/A", "gps_lng": "N/A", "alcohol": "N/A"}

@app.route('/update', methods=['POST'])
def update_data():
    global esp_data
    json_data = request.get_json()
    if json_data:
        with data_lock:
            esp_data.update(json_data)
        print("[+] ESP32 Data Updated:", json_data)
        return jsonify({"status": "updated"})
    return jsonify({"status": "invalid"}), 400

@app.route('/get', methods=['GET'])
def get_data():
    with data_lock:
        return jsonify(esp_data)

def run_flask():
    try:
        app.run(host='0.0.0.0', port=5000)
    except Exception as e:
        print(f"[!] Flask error: {e}")

flask_thread = threading.Thread(target=run_flask)
flask_thread.daemon = True
flask_thread.start()

# ========== DLIB SETUP ==========
print("[*] Loading models...")
try:
    if not os.path.exists("shape_predictor_68_face_landmarks.dat") or not os.path.exists("dlib_face_recognition_resnet_model_v1.dat"):
        raise FileNotFoundError("Model files missing")

    detector = dlib.get_frontal_face_detector()
    shape_predictor = dlib.shape_predictor("shape_predictor_68_face_landmarks.dat")
    face_rec_model = dlib.face_recognition_model_v1("dlib_face_recognition_resnet_model_v1.dat")
    known_embeddings = joblib.load("owner_embeddings.pkl")
except Exception as e:
    print(f"[!] Lỗi tải model hoặc embeddings: {e}")
    GPIO.cleanup()
    exit(1)

LEFT_EYE = list(range(36, 42))
RIGHT_EYE = list(range(42, 48))

def eye_aspect_ratio(eye):
    A = np.linalg.norm(np.array(eye[1]) - np.array(eye[5]))
    B = np.linalg.norm(np.array(eye[2]) - np.array(eye[4]))
    C = np.linalg.norm(np.array(eye[0]) - np.array(eye[3]))
    return (A + B) / (2.0 * C)

def mouth_aspect_ratio_standardized(shape):
    P61 = np.array([shape.part(60).x, shape.part(60).y])
    P62 = np.array([shape.part(61).x, shape.part(61).y])
    P63 = np.array([shape.part(62).x, shape.part(62).y])
    P64 = np.array([shape.part(63).x, shape.part(63).y])
    P65 = np.array([shape.part(64).x, shape.part(64).y])
    P66 = np.array([shape.part(65).x, shape.part(65).y])
    P67 = np.array([shape.part(66).x, shape.part(66).y])
    P68 = np.array([shape.part(67).x, shape.part(67).y])
    vertical = np.linalg.norm(P62 - P68) + np.linalg.norm(P63 - P67) + np.linalg.norm(P64 - P66)
    horizontal = 2.0 * np.linalg.norm(P61 - P65)
    return vertical / horizontal if horizontal != 0 else 0.0

def is_match(embedding, known_embeddings, threshold=0.4):
    return any(np.linalg.norm(embedding - known) < threshold for known in known_embeddings)

# ========== AUDIO ==========
try:
    pygame.mixer.init()
    alarm_channel = pygame.mixer.Channel(0)
    stop_channel = pygame.mixer.Channel(1)
    alcohol_channel = pygame.mixer.Channel(2)
    reminder_channel = pygame.mixer.Channel(3)
    alarm_sound = pygame.mixer.Sound("alert.mp3")
    stop_reminder_sound = pygame.mixer.Sound("bip.mp3")
    alcohol_alert_sound = pygame.mixer.Sound("alcohol.mp3")
    reminder_sound = pygame.mixer.Sound("reminder.mp3")
except Exception as e:
    print(f"[!] Pygame init failed: {e}")
    GPIO.cleanup()
    exit(1)

# ========== CAMERA ==========
try:
    picam2 = Picamera2()
    picam2.configure(picam2.create_preview_configuration(main={'format': 'RGB888', 'size': (640, 480)}))
    picam2.start()
    time.sleep(2)
except Exception as e:
    print(f"[!] Picamera init failed: {e}")
    GPIO.cleanup()
    exit(1)

closed_eye_time = 0
driving_time = 0
last_seen_time = None
warning_start_time = None
reminder_display_start_time = None
last_alarm_time = 0
last_alcohol_alert = 0
last_violate_alcohol_time = 0

# ========== YAWN LOGIC WITH COOLDOWN ==========
yawn_count = 0
is_yawning = False
yawn_start_time = 0
last_yawn_time = 0
MAR_ENTER = 0.62
MAR_EXIT = 0.55
YAWN_MIN_DURATION = 1
YAWN_COOLDOWN = 3
EAR_THRESHOLD = 0.25

def get_esp32_data():
    try:
        r = requests.get("http://localhost:5000/get", timeout=1)
        return r.json()
    except:
        return {"heart_rate": "N/A", "gps_lat": "N/A", "gps_lng": "N/A", "alcohol": "N/A"}

try:
    print("[*] Bắt đầu hệ thống. Nhấn 'q' để thoát.")

    # Vị trí mới cho hiển thị
    x_left = 20
    y_top = 40
    line_gap = 30
    x_right = 400
    font_scale = 0.7
    font_thickness = 1

    while True:
        frame = picam2.capture_array()
        gray = cv2.cvtColor(frame, cv2.COLOR_RGB2GRAY)
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)

        key = read_keypad()
        if key:
            if key == "D":
                keypad_mode = None
                keypad_buffer = ""
                editing_index = -1
            elif keypad_mode:
                if key in "0123456789" and editing_index != -1:
                    index_map = {0: (0, 2), 1: (2, 4), 2: (4, 6)}
                    start, end = index_map[editing_index]
                    current = keypad_buffer[start:end].rjust(2, "0")
                    current = current[1] + key
                    keypad_buffer = keypad_buffer[:start] + current + keypad_buffer[end:]
                if key == '#':
                    try:
                        value = int(keypad_buffer[editing_index * 2:editing_index * 2 + 2])
                        if editing_index == 0:
                            hour = value if 0 <= value <= 23 else 0
                            if hour == 0: keypad_buffer = "00" + keypad_buffer[2:]
                        elif editing_index == 1:
                            minute = value if 0 <= value <= 59 else 0
                            if minute == 0: keypad_buffer = keypad_buffer[0:2] + "00" + keypad_buffer[4:]
                        elif editing_index == 2:
                            second = value if 0 <= value <= 59 else 0
                            if second == 0: keypad_buffer = keypad_buffer[0:4] + "00"
                    except:
                        keypad_buffer = "000000"
                    editing_index = (editing_index + 1) % 3
                elif key == "*":
                    if len(keypad_buffer) == 6:
                        hh = int(keypad_buffer[0:2])
                        mm = int(keypad_buffer[2:4])
                        ss = int(keypad_buffer[4:6])
                        total = hh * 3600 + mm * 60 + ss
                        if keypad_mode == "alcohol":
                            reminder_interval_alcohol = total
                            last_reminder_time_alcohol = time.time()
                        elif keypad_mode == "drive":
                            reminder_max_drive_duration = total
                            driving_time = 0
                    keypad_mode = None
                    keypad_buffer = ""
                    editing_index = -1
            else:
                if key == "A":
                    keypad_mode = "alcohol"
                    keypad_buffer = "000000"
                    editing_index = 0
                elif key == "B":
                    keypad_mode = "drive"
                    keypad_buffer = "000000"
                    editing_index = 0
            time.sleep(0.3)

        if time.time() - last_blink_time > 0.5:
            blink_state = not blink_state
            last_blink_time = time.time()

        faces = detector(gray)
        driver_found = False

        for face in faces:
            shape = shape_predictor(rgb_frame, face)
            descriptor = np.array(face_rec_model.compute_face_descriptor(rgb_frame, shape))
            matched = is_match(descriptor, known_embeddings)

            x1, y1, x2, y2 = face.left(), face.top(), face.right(), face.bottom()
            label = "Driver" if matched else "Unknown"
            color = (0, 255, 0) if matched else (0, 0, 255)

            cv2.rectangle(rgb_frame, (x1, y1), (x2, y2), color, 2)
            cv2.putText(rgb_frame, label, (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.8, color, 2)

            if matched:
                driver_found = True
                now = time.time()
                if last_seen_time is not None:
                    driving_time += now - last_seen_time
                last_seen_time = now

                left_eye = [(shape.part(i).x, shape.part(i).y) for i in LEFT_EYE]
                right_eye = [(shape.part(i).x, shape.part(i).y) for i in RIGHT_EYE]
                EAR = (eye_aspect_ratio(left_eye) + eye_aspect_ratio(right_eye)) / 2.0

                # Ngủ gật
                if EAR < 0.22 and time.time() - last_alarm_time > 5:
                    if closed_eye_time == 0:
                        closed_eye_time = time.time()
                    elif time.time() - closed_eye_time > 3:
                        # Cảnh báo buồn ngủ giữa màn hình
                        cv2.putText(rgb_frame, "CANH BAO NGU GAT", (150, 200), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 255), 3)
                        if not alarm_channel.get_busy():
                            alarm_channel.play(alarm_sound)
                            last_alarm_time = time.time()
                        folder_path = os.path.join("images", datetime.datetime.now().strftime("%Y-%m-%d"))
                        os.makedirs(folder_path, exist_ok=True)
                        filename = f"drowsy_{datetime.datetime.now().strftime('%H-%M-%S')}.jpg"
                        cv2.imwrite(os.path.join(folder_path, filename), rgb_frame)
                        # === Tao anh ===
                        now = datetime.datetime.now()
                        filename = f"drowsy_{now.strftime('%H-%M-%S')}.jpg"
                        full_path = os.path.join(folder_path, filename)
                        cv2.imwrite(full_path, rgb_frame)
                        print(f"[+] Da luu anh: {full_path}")
                else:
                    closed_eye_time = 0

                # ========================== YAWN LOGIC HYSTERESIS + EAR + COOLDOWN ==========================
                MAR = mouth_aspect_ratio_standardized(shape)
                current_time = time.time()
                if not is_yawning:
                    if MAR > MAR_ENTER and EAR < EAR_THRESHOLD:
                        if (current_time - last_yawn_time) > YAWN_COOLDOWN:
                            yawn_start_time = current_time
                            is_yawning = True
                else:
                    if MAR > MAR_ENTER and EAR < EAR_THRESHOLD:
                        if (current_time - yawn_start_time) >= YAWN_MIN_DURATION:
                            yawn_count += 1
                            last_yawn_time = current_time
                            is_yawning = False
                    elif MAR < MAR_EXIT or EAR >= EAR_THRESHOLD:
                        is_yawning = False

        if not driver_found:
            last_seen_time = None

        if driving_time >= reminder_max_drive_duration:
            if warning_start_time is None:
                warning_start_time = time.time()
            if time.time() - warning_start_time <= 7:
                if not stop_channel.get_busy():
                    stop_channel.play(stop_reminder_sound)
                # Cảnh báo nghỉ ngơi giữa màn hình
                cv2.putText(rgb_frame, "VUI LONG NGHI NGOI", (180, 200), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 255), 5)
            else:
                warning_start_time = None
                driving_time = 0

        # ========== PHẦN HIỂN THỊ ==========
        elapsed_time_str = str(datetime.timedelta(seconds=int(driving_time)))

        # Thông tin trạng thái bên trái
        cv2.putText(rgb_frame, f"Thoi gian lai xe: {elapsed_time_str}", (x_left, y_top), cv2.FONT_HERSHEY_SIMPLEX, font_scale, (255,255,0), font_thickness)
        cv2.putText(rgb_frame, f"So lan ngap: {yawn_count}", (x_left, y_top + line_gap), cv2.FONT_HERSHEY_SIMPLEX, font_scale, (0,255,255), font_thickness)

        data = get_esp32_data()
        cv2.putText(rgb_frame, f"Heart rate: {data['heart_rate']}", (x_left, y_top + 2*line_gap), cv2.FONT_HERSHEY_SIMPLEX, font_scale, (255,255,255), font_thickness)
        cv2.putText(rgb_frame, f"Alcohol: {data['alcohol']}", (x_left, y_top + 3*line_gap), cv2.FONT_HERSHEY_SIMPLEX, font_scale, (0,255,0), font_thickness)
        cv2.putText(rgb_frame, f"GPS: {data['gps_lat']}, {data['gps_lng']}", (x_left, y_top + 4*line_gap), cv2.FONT_HERSHEY_SIMPLEX, font_scale, (0,255,255), font_thickness)

        # Ngày giờ bên phải phía trên
        now = datetime.datetime.now()
        date_str = now.strftime("%Y-%m-%d")
        time_str = now.strftime("%H:%M:%S")
        cv2.putText(rgb_frame, f"Date: {date_str}", (x_right, y_top), cv2.FONT_HERSHEY_SIMPLEX, font_scale, (255,255,0), font_thickness)
        cv2.putText(rgb_frame, f"Time: {time_str}", (x_right, y_top + line_gap), cv2.FONT_HERSHEY_SIMPLEX, font_scale, (255,255,0), font_thickness)

        # Cảnh báo kiểm tra cảm biến giữa màn hình phía dưới
        current_time = time.time()
        if current_time - last_reminder_time_alcohol >= reminder_interval_alcohol:
            last_reminder_time_alcohol = current_time
            reminder_display_start_time = current_time
            if not reminder_channel.get_busy():
                reminder_channel.play(reminder_sound)

        if reminder_display_start_time and (current_time - reminder_display_start_time <= 3):
            cv2.putText(rgb_frame, "VUI LONG DO LAI NONG DO CON VA NHIP TIM", (60, 210), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 0, 255), 5)

        try:
            alcohol_val = float(data["alcohol"])
            if alcohol_val >= 0.12  and time.time() - last_alcohol_alert > 7:
                last_violate_alcohol_time = time.time()
                if not alcohol_channel.get_busy():
                    alcohol_channel.play(alcohol_alert_sound)
                    last_alcohol_alert = time.time()
        except:
            pass
        if time.time() - last_violate_alcohol_time <= 5:
         cv2.putText(rgb_frame, "VI PHAM NONG DO CON!", (100, 240),cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 255), 2)		                       		
        # Hiển thị chế độ cài đặt keypad phía dưới
        if keypad_mode:
            label = "CAI DAT CHU KI DO LAI" if keypad_mode == "alcohol" else "CAI DAT LAI XE"
            if len(keypad_buffer) == 6:
                hh = keypad_buffer[0:2]
                mm = keypad_buffer[2:4]
                ss = keypad_buffer[4:6]
                if editing_index == 0 and not blink_state:
                    hh = "  "
                elif editing_index == 1 and not blink_state:
                    mm = "  "
                elif editing_index == 2 and not blink_state:
                    ss = "  "
                time_preview = f"{hh}:{mm}:{ss}"
            else:
                padded = keypad_buffer.ljust(6, '_')
                hh = padded[0:2]
                mm = padded[2:4]
                ss = padded[4:6]
                time_preview = f"{hh}:{mm}:{ss}"
            cv2.putText(rgb_frame, f"{label}: {time_preview}", (80, 440), cv2.FONT_HERSHEY_SIMPLEX, font_scale, (0,255,255), font_thickness)

        cv2.imshow("Driver Monitoring System", rgb_frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

except KeyboardInterrupt:
    print("\n[*] KeyboardInterrupt. Dừng hệ thống.")

finally:
    cv2.destroyAllWindows()
    try:
        picam2.close()
    except:
        pass
    GPIO.cleanup()
    print("[*] Dừng chương trình.")
