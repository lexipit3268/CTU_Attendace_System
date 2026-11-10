# Hệ Thống Điểm Danh & Ghi Danh Sinh Viên IoT (ESP32 + RFID/NFC)

Hệ thống quản lý, ghi danh và điểm danh sinh viên sử dụng ESP32 kết nối Web Server qua REST API, lưu trữ cơ sở dữ liệu SQLite và bảng điều khiển trực tiếp theo thời gian thực (Real-time Dashboard).

---

## 1. Cấu Trúc Dự Án

- `ESP32_ATTENDANCE_SYSTEM/`: Mã nguồn Arduino cho ESP32 (`ESP32_ATTENDANCE_SYSTEM.ino`).
- `server/`: Ứng dụng Web Dashboard & REST API Backend (Node.js, TypeScript, Express, SQLite, ExcelJS).
  - `public/index.html`: Giao diện Dashboard quản lý tuân thủ thiết kế [DESIGN.md](DESIGN.md).
  - `src/routes/api.ts`: Các endpoint xử lý quẹt thẻ, đăng ký sinh viên, xuất Excel.
  - `src/db.ts`: Khởi tạo SQLite và nạp dữ liệu ban đầu.
- `run_server.bat`: Script khởi chạy nhanh Server bằng một click trên Windows.
- `SoDoMach.txt`: Sơ đồ chân kết nối phần cứng chuẩn.

---

## 2. Sơ Đồ Đấu Dây Phần Cứng (Theo SoDoMach.txt)

| Linh Kiện        | Chân Linh Kiện   | Chân ESP32 (GPIO)   | Ghi Chú                          |
| :--------------- | :--------------- | :------------------ | :------------------------------- |
| **RFID RC522**   | SDA / SS         | GPIO 5              | Giao tiếp SPI                    |
|                  | SCK              | GPIO 18             | SPI Clock                        |
|                  | MOSI             | GPIO 23             | SPI Data In                      |
|                  | MISO             | GPIO 19             | SPI Data Out                     |
|                  | RST              | GPIO 4              | Reset                            |
|                  | 3.3V / GND       | 3V3 / GND           | **Không cấp nguồn 5V cho RC522** |
| **LCD 16x2 I2C** | SDA              | GPIO 21             | Giao tiếp I2C (Địa chỉ 0x27)     |
|                  | SCL              | GPIO 22             | I2C Clock                        |
|                  | VCC / GND        | 5V (hoặc 3V3) / GND |                                  |
| **Buzzer**       | Chân (+)         | GPIO 25             | Phát âm thanh thông báo          |
|                  | Chân (-)         | GND                 |                                  |
| **LED Đỏ**       | Chân (+) Anode   | GPIO 26             | Nối qua điện trở 220Ω - 330Ω     |
|                  | Chân (-) Cathode | GND                 | Đèn báo trạng thái               |

---

## 3. Hướng Dẫn Khởi Chạy

### Bước 1: Cấu Hình Wi-Fi Cho ESP32

1. Sao chép `ESP32_ATTENDANCE_SYSTEM/WifiConfig.example.h` thành `ESP32_ATTENDANCE_SYSTEM/WifiConfig.h`.
2. Mở `WifiConfig.h` và điền thông tin mạng Wi-Fi mà ESP32 sẽ kết nối:

   ```cpp
   #define WIFI_SSID "ten-wifi-cua-ban"
   #define WIFI_PASS "mat-khau-wifi-cua-ban"
   ```

3. Lưu file. Sketch điểm danh chính và sketch `TEST_WIFI_CONNECTION` cùng đọc cấu hình này; không cần khai báo lại thông tin Wi-Fi trong các file `.ino`.

`WifiConfig.h` chứa thông tin riêng và đã được thêm vào `.gitignore`. Không đưa file này lên Git hoặc chia sẻ công khai. Nếu mật khẩu thật từng được commit hoặc chia sẻ, hãy đổi mật khẩu Wi-Fi; `.gitignore` không xóa thông tin khỏi lịch sử Git.

### Bước 2: Chuẩn bị Driver & Thiết lập Ardunio

- Cài đặt đủ Driver trong thư mục [ESP_Driver](./ESP_Driver/)
  - [CH34x_Install_Windows_v3_4](./ESP_Driver/CH34x_Install_Windows_v3_4.EXE)
  - [CP210xVCPInstaller_x64](./ESP_Driver/CP210xVCPInstaller_x64.exe)
  - [Addition_Board](./ESP_Driver/ESP8266_board.txt)

- Thiết lập Arduino
  - Cài đặt Board: esp32 by Espressif Systems
  - Cài đặt thư viện: LiquidCrystal_I2C, MFRC522, ArduinoHttpClient

### Bước 3: Bật Mobile Hotspot Trên Laptop

- Tên Wi-Fi (SSID): `...`
- Mật khẩu: `...`
  _(Địa chỉ IP mặc định của card hotspot Windows là `192.168.137.1`)._

### Bước 4: Khởi Động Web Server

Nhấp đúp chuột vào file `run_server.bat` (hoặc mở terminal tại thư mục `server` và chạy `pnpm start`).

- Giao diện quản lý: [http://localhost:3000](http://localhost:3000)
- Endpoint ESP32: `http://192.168.137.1:3000/api/attendance/scan`

### Bước 5: Nạp Code Vào ESP32

0. Kiểm tra kết nối trong Device Manager

- Ghim cắm dây nguồn vào
- Mở Device Manager > Ports
- Nếu thấy `Silicon... (COMx)` nghĩa là đã nhận driver và có thể tiến hành kết nối bình thường

1. Mở `ESP32_ATTENDANCE_SYSTEM/ESP32_ATTENDANCE_SYSTEM.ino` trong Arduino IDE.
2. Chọn board: **ESP32 Dev Module** và chọn Port với COMx tương ứng
3. Cắm cáp Micro-USB và nhấn **Upload**.

---

## 4. Các Tính Năng Hoạt Động

1. **Điểm danh (Attendance)**:
   - Quẹt thẻ NFC/RFID hợp lệ $\rightarrow$ LCD hiện `DIEM DANH OK! [MSSV] [Tên]`, còi kêu bíp, LED sáng, Dashboard cập nhật lượt quẹt ngay lập tức (SSE Realtime).
   - Quẹt thẻ chưa đăng ký $\rightarrow$ LCD hiện `TU CHOI TRUY CAP`, còi và LED chớp cảnh báo 3 lần.
   - Nếu Server mất mạng $\rightarrow$ Hệ thống tự động chuyển sang cơ chế Offline dự phòng, tiếp tục điểm danh dựa trên danh sách sinh viên nội bộ.

2. **Ghi danh thẻ mới (Enrollment)**:
   - Trên Dashboard, nhấn **"Bật Chế Độ Ghi Danh"**.
   - Đưa thẻ NFC bất kỳ vào đầu đọc ESP32.
   - Giao diện Web lập tức bắt được mã UID, tự động mở cửa sổ thêm sinh viên và điền sẵn mã UID.
   - Nhập MSSV, Họ và Tên lót, Tên, Lớp rồi nhấn **Lưu Sinh Viên**. Thẻ sẽ có hiệu lực ngay lập tức.

3. **Xuất Báo Cáo**:
   - Tại tab "Báo Cáo & Thống Kê", chọn ngày (hoặc để trống để lấy tất cả) rồi nhấn **Tải File Excel (.xlsx)**.
