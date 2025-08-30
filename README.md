ESP32 Bluetooth Car với FreeRTOS
******Giới thiệu

Dự án này điều khiển xe 2 động cơ (dùng driver L298N) thông qua Bluetooth với ESP32.
Chương trình sử dụng FreeRTOS để chạy song song nhiều tác vụ:

Nhận lệnh từ Bluetooth.

Nhận lệnh từ Serial Monitor.

Điều khiển motor (task ưu tiên cao).

Hiển thị trạng thái hệ thống định kỳ.

Xe có thể tiến, lùi, rẽ trái, rẽ phải, dừng lại và thay đổi tốc độ bằng phím bấm.

******Phần cứng yêu cầu:

ESP32 DevKit.

Module L298N (điều khiển 2 động cơ).

Động cơ DC x4 (bánh xe).

Nguồn pin 7.4V hoặc 12V (tùy loại động cơ).

Điện thoại có Bluetooth Classic (Android/iOS).

![alt text](image.png)

******Cài đặt phần mềm

Cài Arduino IDE và chọn board ESP32 Dev Module.

Thêm thư viện:

BluetoothSerial.h (có sẵn trong ESP32 Arduino core).

FreeRTOS (có sẵn trong ESP32).

Upload code lên ESP32.

******Cách sử dụng

Dùng điện thoại kết nối Bluetooth đến thiết bị có tên: ESP32_BT_CAR

Sử dụng app Bluetooth Terminal để gửi lệnh.

******Các lệnh điều khiển

F / f	Tiến
B / b	Lùi
L / l	Rẽ trái
R / r	Rẽ phải
S / s	Dừng
1-9	Điều chỉnh tốc độ (1 = chậm, 9 = nhanh nhất)

FreeRTOS Tasks

| Task            | Chức năng                         | Core   | Priority |
| --------------- | --------------------------------- | ------ | -------- |
| `bluetoothTask` | Nhận lệnh từ Bluetooth            | Core 0 | 2        |
| `serialTask`    | Nhận lệnh từ Serial Monitor       | Core 0 | 2        |
| `motorTask`     | Xử lý điều khiển motor            | Core 1 | 3        |
| `statusTask`    | In trạng thái hệ thống mỗi 5 giây | Core 0 | 1        |
