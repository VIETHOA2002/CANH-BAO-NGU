🚗 Hệ Thống Cảnh Báo Ngủ Gật & Giám Sát An Toàn Tài Xế Ô Tô
📌 Mô tả ngắn:
Hệ thống tích hợp AI và cảm biến nhằm theo dõi tình trạng tỉnh táo và sức khỏe của tài xế ô tô, cảnh báo buồn ngủ, nồng độ cồn, và ghi nhận hành trình để đảm bảo an toàn giao thông.

📖 Giới thiệu
Đây là đồ án tốt nghiệp được thực hiện bởi sinh viên ngành Công nghệ Kỹ thuật Điện tử - Viễn thông tại Đại học Sư phạm Kỹ thuật TP.HCM.

Hệ thống sử dụng:

Camera + AI (OpenCV, Dlib) để nhận diện khuôn mặt, phát hiện nhắm mắt kéo dài.

Cảm biến nhịp tim (XD-58C) để theo dõi sức khỏe tài xế.

Cảm biến nồng độ cồn (SEN0376) để phát hiện vi phạm nồng độ cồn.

GPS (NEO-M8N) để ghi nhận vị trí, tốc độ và gợi ý điểm dừng.

ESP32 + Raspberry Pi 4 làm bộ xử lý trung tâm.

Kết nối WebServer để giám sát và lưu trữ dữ liệu từ xa.

🔧 Tính năng chính
👁️ Nhận diện dấu hiệu buồn ngủ qua camera và AI (nhắm mắt, gục đầu).

❤️ Theo dõi nhịp tim thời gian thực.

🍺 Cảnh báo nếu nồng độ cồn vượt mức cho phép.

📍 Ghi nhận vị trí GPS và gợi ý điểm nghỉ sau 4h lái xe.

🔊 Cảnh báo âm thanh khi phát hiện bất thường.

☁️ Đồng bộ dữ liệu lên webserver để theo dõi từ xa.
📦 Cấu trúc hệ thống
bash
Sao chép
Chỉnh sửa
📁 /src
├── /esp32-code        # Mã điều khiển cảm biến, cảnh báo, truyền dữ liệu
├── /raspberry-ai      # Phân tích ảnh, phát hiện buồn ngủ
├── /webserver         # Giao diện quản lý dữ liệu từ xa
└── /hardware-docs     # Sơ đồ nguyên lý, bố trí linh kiện
💻 Công nghệ sử dụng
Phần mềm & thư viện: Python (OpenCV, Dlib), Arduino IDE, ESP-IDF
Phần cứng chính:

Raspberry Pi 4
ESP32
Camera Pi
GPS NEO-M8N
Pulse Sensor XD-58C
Alcohol Sensor SEN0376
📈 Kết quả
Nhận diện buồn ngủ chính xác trong điều kiện ánh sáng ổn định.
Hệ thống hoạt động ổn định khi đo nhịp tim, GPS và cồn.
Dữ liệu được truyền và hiển thị trên giao diện web đơn giản.
Góp phần nâng cao an toàn khi lái xe đường dài, đặc biệt vào ban đêm.




<img width="238" height="341" alt="image" src="https://github.com/user-attachments/assets/aa95fb0e-1cdf-4ebe-87eb-1de14ca5b674" />

