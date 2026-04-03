# TCS34725_PA67 - Board gửi mã màu

## Mô tả
Project dùng STM32F103C8 với framework CMSIS/StdPeriph để:
- Đọc cảm biến màu TCS34725 qua I2C1
- Phân loại màu
- Gửi mã màu sang board nhận qua 3 chân GPIO
- Gửi log debug qua UART1

## Vai trò truyền thông màu
- Project này là **board gửi**
- Xuất tín hiệu sang board nhận bằng 3 chân:
  - `PA0` = BIT0
  - `PA1` = BIT1
  - `PA2` = VALID

## Mã màu gửi đi
- `00` = ĐỎ
- `01` = XANH DƯƠNG
- `10` = XANH LÁ
- `11` = TRẮNG
- Nếu không khớp màu / dữ liệu không hợp lệ thì kéo `VALID = 0`

## Kết nối cảm biến màu
- I2C1:
  - `PB6` = SCL
  - `PB7` = SDA
- TCS34725 VCC -> `3.3V`
- TCS34725 GND -> `GND`
- Nên có pull-up cho SDA/SCL nếu module chưa tích hợp sẵn

## UART debug
- `PA9` = TX
- `PA10` = RX
- Baudrate = `115200`

## Hành vi
- Khởi tạo cảm biến TCS34725
- Đọc giá trị RGB/Clear
- Chuẩn hóa và phân loại màu
- Xuất mã màu qua GPIO để board nhận đọc
- In log trạng thái qua UART

## Chức năng mã nguồn
- `src/main.c`: đọc cảm biến, phân loại màu, xuất mã màu qua GPIO
- `src/usart.c`: cấu hình UART1 và hàm gửi dữ liệu serial
- `src/i2c.c`: cấu hình I2C1
- `src/tcs34725.c`: driver cảm biến màu TCS34725

## Ghi chú đấu nối với board nhận
- Nối từ board gửi sang board nhận:
  - `PA0 (gửi)` -> `PA3 (nhận)`
  - `PA1 (gửi)` -> `PA4 (nhận)`
  - `PA2 (gửi)` -> `PA5 (nhận)`
- Nhớ nối chung GND giữa hai board
