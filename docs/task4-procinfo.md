# Báo cáo task 4 procinfo

## Mục tiêu và giao diện

System call `int procinfo(int pid, struct procinfo *info)` lấy thông tin của tiến trình theo PID và trả dữ liệu về user-space. Giá trị trả về là 0 khi thành công, -1 khi PID không hợp lệ, không tìm thấy tiến trình hoặc vùng nhớ đích không ghi được.

ABI dùng chung trong `kernel/procinfo.h` gồm `pid`, `ppid`, `state`, `uint64 sz` và `char name[16]`. `sz` là kích thước không gian địa chỉ theo byte. Tên chứa tối đa 15 ký tự và ký tự kết thúc chuỗi. Các trạng thái theo kernel/proc.h là UNUSED=0, USED=1, SLEEPING=2, RUNNABLE=3, RUNNING=4 và ZOMBIE=5.

## Luồng xử lý

Prototype trong user/user.h và stub sinh từ user/usys.pl đưa lời gọi qua ecall. Kernel tra SYS_procinfo trong kernel/syscall.h và bảng syscall trong kernel/syscall.c để gọi sys_procinfo. Trên nhánh feat/integration, SYS_procinfo=23.

sys_procinfo lấy PID bằng argint, địa chỉ đích bằng argaddr, xóa trắng struct kết quả rồi gọi getprocinfo. Hàm getprocinfo trong kernel/proc.c duyệt bảng proc[NPROC], bỏ phần tử UNUSED và tìm đúng PID. Hàm chép PID, PID cha, trạng thái, kích thước bộ nhớ và tên vào struct kết quả. Các khóa được thả trước khi copyout ghi kết quả vào bảng trang của tiến trình gọi.

Tiến trình ZOMBIE vẫn có thể được tìm thấy cho tới khi cha thu hồi bằng wait. Khi vùng đệm đi qua một trang không hợp lệ, copyout có thể đã ghi phần đầu trước khi trả -1.

## Xử lý lỗi dữ liệu dư

Struct có byte đệm căn chỉnh và mảng name có thể còn các byte sau ký tự kết thúc chuỗi. Gán các trường riêng lẻ không khởi tạo các byte đó. Trước khi điền dữ liệu, sys_procinfo gọi `memset(&info, 0, sizeof(info))` để toàn bộ vùng copyout đều được khởi tạo. Kiểm tra so sánh cả struct với một bản đã xóa trắng giúp phát hiện byte dư ngoài các giá trị trường thông thường.

## Đồng bộ dữ liệu giữa các CPU

getprocinfo giữ wait_lock để đọc quan hệ cha–con và lấy p->lock khi đọc từng phần tử. Thứ tự lấy khóa là wait_lock rồi p->lock.

Các đường ghi được đồng bộ với khóa của đường đọc. Trong exec, tên chương trình và kích thước ảnh mới được công bố cùng nhau trong một vùng p->lock. Việc chuyển bảng trang và cập nhật thanh ghi khởi chạy cũng nằm trong đoạn công bố ngắn này. Việc tải ELF, cấp phát trang, copyout tham số và giải phóng ảnh cũ nằm ngoài vùng khóa.

Trong growproc, cấp phát hoặc thu hồi trang chạy trước, sau đó kích thước mới được công bố dưới p->lock. Tiến trình đang thực thi có thể đọc kích thước của chính nó vì chỉ tiến trình đó thay đổi không gian địa chỉ của mình. Những đường khởi tạo hoặc thu hồi khác trong allocproc, userinit, fork và freeproc vốn đã giữ khóa tiến trình tương ứng khi sửa metadata.

Nhờ đồng bộ cả đường đọc và đường ghi, procinfo đọc được một cặp tên/kích thước trước hoặc sau lần công bố của exec. Các giá trị mô tả thời điểm lấy khóa và có thể thay đổi sau khi syscall trả về.

## Kiểm thử

`test_procinfo` gồm 29 kiểm tra. Các nhóm bao gồm thông tin bản thân, byte đệm và đuôi name, tăng/giảm bộ nhớ bằng sbrk, PID lỗi, quan hệ cha–con có đồng bộ pipe, PID đã thu hồi, địa chỉ ngoài user-space, trang không ánh xạ và buffer cắt qua trang không hợp lệ.

`procstress` kiểm tra trên QEMU nhiều CPU. Hai chương trình pi_exec_a và pi_exec_b được biên dịch từ user/pi_exec.c với kích thước vùng dữ liệu khác nhau. Chương trình cha đo kích thước từng ảnh bằng sbrk(0) trong một tiến trình con riêng. Tiến trình con của lượt stress đổi qua lại giữa hai ảnh bằng 400 lần exec, còn cha liên tục gọi procinfo và xác nhận tên/kích thước khớp một trong hai ảnh đã đo. Cả hai ảnh phải được quan sát.

Lượt stress heap cho con thực hiện 1.000 cặp sbrk tăng/giảm 4.096 byte. Cha xác nhận kích thước chỉ nhận giá trị ban đầu hoặc giá trị tăng thêm một trang, đồng thời kiểm tra PID và PPID. Cả hai kích thước phải được quan sát. Pipe giữ tiến trình con ở trạng thái đã sẵn sàng trước khi bắt đầu lấy mẫu. Mỗi pha lấy mẫu trong 100 tick và cha wait để kiểm tra trạng thái kết thúc của con.

Chạy trong Linux có GCC RISC-V và QEMU:

```sh
make clean
make
make qemu
```

Trong shell xv6:

```text
test_procinfo
trace 8388608 test_procinfo
procstress
usertests
```

Mask 8388608 tương ứng 1 << 23 trên nhánh tích hợp. Trên nhánh feat-procinfo, syscall vẫn mang số 22 và bộ kiểm tra trực tiếp được chạy bằng test_procinfo cùng procstress.

## Kết quả ngày 07/11/2026

Môi trường Debian 12/WSL2, GCC RISC-V 12.2.0, QEMU 7.2.22, 3 CPU, 128 MiB RAM. Commit nền của nhánh tích hợp là 384a01e.

| Kiểm tra trên feat/integration | Kết quả |
|---|---|
| test_procinfo | Đạt 29/29, gồm byte đệm và phần dư name bằng 0 |
| trace 8388608 test_procinfo | Đạt 29/29, có syscall trả 0 và -1 đúng các trường hợp |
| procstress lượt 1 | Đạt, 117.357 mẫu exec và 130.548 mẫu heap |
| procstress lượt 2 | Đạt, 115.069 mẫu exec và 130.921 mẫu heap |
| usertests sbrkmuch | ALL TESTS PASSED |
| usertests đầy đủ | ALL TESTS PASSED, gồm test nhanh và test chậm, khoảng 404 giây |

Hai lượt stress thực hiện tổng cộng 800 lần chuyển ảnh bằng exec và 2.000 cặp tăng/giảm heap. Bộ kiểm tra quan sát đủ cả hai ảnh và cả hai kích thước heap. Các lỗi byte dư và đồng bộ metadata được sửa ở cả đường ghi và đường đọc, kết hợp kiểm chứng lúc chạy trên nhiều CPU.

- [Log kiểm chứng nhánh tích hợp](testing/procinfo-fixes-20261107.txt).
- [Kết quả máy đọc được](testing/procinfo-fixes-20261107.json).

Các lỗi tree duyệt cây sâu và trace trả mã thành công khi exec thất bại trong danh sách review được giao cho các phần tương ứng. Phần cập nhật này xử lý hai mục procinfo số 2 và số 3. Báo cáo này thay thế mô tả giới hạn đồng bộ và lỗi dữ liệu dư của procinfo trong các bản báo cáo trước.
