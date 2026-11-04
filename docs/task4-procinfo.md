# Task 4 — System call `procinfo`

## 1. Mục tiêu và giao diện

System call `int procinfo(int pid, struct procinfo *info)` lấy thông tin của tiến trình có PID chỉ định và chép kết quả vào vùng nhớ của chương trình gọi. Hàm trả về `0` khi thành công, `-1` nếu PID không hợp lệ, không tìm thấy tiến trình hoặc không thể ghi kết quả vào bộ nhớ user-space.

Cấu trúc dùng chung được định nghĩa trong `kernel/procinfo.h`:

```c
struct procinfo {
  int pid;
  int ppid;
  int state;
  uint64 sz;
  char name[16];
};
```

`pid` là mã tiến trình; `ppid` là mã tiến trình cha (bằng 0 nếu không có cha); `state` là trạng thái; `sz` là kích thước không gian địa chỉ theo byte, không phải số byte RAM vật lý thực tế; `name` là tên tiến trình, tối đa 15 ký tự và ký tự kết thúc chuỗi. Các giá trị trạng thái theo `kernel/proc.h`: UNUSED=0, USED=1, SLEEPING=2, RUNNABLE=3, RUNNING=4, ZOMBIE=5. Tiến trình ZOMBIE vẫn có thể được tìm thấy cho tới khi cha thu hồi bằng `wait()`.

## 2. Cấu trúc tiến trình trong xv6

Kernel lưu bảng `proc[NPROC]` trong `kernel/proc.c`. Mỗi phần tử `struct proc` chứa thông tin lập lịch, PID, con trỏ cha, kích thước bộ nhớ, bảng trang, trapframe, context, file đang mở và tên tiến trình.

`p->lock` bảo vệ các trường như PID và trạng thái; `wait_lock` bảo vệ quan hệ cha–con. Khi cần cả hai, phải lấy `wait_lock` trước `p->lock`, thống nhất với các đường xử lý `wait()` và `exit()` để tránh đảo thứ tự khóa.

## 3. Luồng thực hiện

1. Chương trình user gọi `procinfo(pid, &info)` qua prototype trong `user/user.h`.
2. `entry("procinfo")` trong `user/usys.pl` sinh stub đặt số syscall vào thanh ghi a7 và thực hiện `ecall`.
3. Kernel tra `SYS_procinfo` trong `kernel/syscall.h` và bảng `syscalls[]` trong `kernel/syscall.c`, rồi gọi `sys_procinfo()`.
4. `sys_procinfo()` lấy PID bằng `argint(0, &pid)` và địa chỉ đích bằng `argaddr(1, &addr)`. Đọc địa chỉ tham số không đồng nghĩa với xác thực khả năng ghi vào địa chỉ đó.
5. Kernel xóa trắng struct kết quả rồi gọi `getprocinfo()`.
6. `getprocinfo()` giữ `wait_lock`, duyệt bảng tiến trình và lần lượt giữ `p->lock`. Khi gặp PID phù hợp và trạng thái khác UNUSED, hàm chép các trường vào struct kết quả, thả khóa và trả về 0. Nếu không tìm thấy, trả về -1.
7. Sau khi đã thả các khóa, `copyout()` chép struct qua bảng trang của tiến trình gọi. Lỗi ghi vào bộ nhớ user được chuyển thành giá trị trả về -1.

Việc tách phần duyệt bảng tiến trình vào `kernel/proc.c` giúp giữ thao tác trên bảng `proc` trong mô-đun quản lý tiến trình. `sys_procinfo()` phụ trách giao diện syscall và truyền dữ liệu giữa kernel/user.

## 4. An toàn dữ liệu và giới hạn

Trước khi điền dữ liệu, `memset(&info, 0, sizeof(info))` xóa cả byte đệm căn chỉnh và phần dư của `name`. Trên ABI RISC-V 64-bit đang dùng, struct có byte đệm giữa `state` và `sz`. Nếu chỉ gán các trường rồi chép toàn bộ struct, byte đệm và phần dư chuỗi có thể làm lộ dữ liệu cũ trên stack kernel.

Kernel không ghi trực tiếp qua con trỏ do user cung cấp. `copyout()` kiểm tra ánh xạ và quyền ghi. Trong bản xv6 này, địa chỉ 0 thuộc vùng mã không cho ghi nên test con trỏ null bị từ chối. Nếu vùng đệm đi qua hai trang và trang sau không hợp lệ, phần đầu có thể đã được ghi trước khi hàm trả -1; API không cam kết ghi toàn bộ hoặc không ghi gì.

Thông tin là kết quả quan sát tại thời điểm truy vấn. Trạng thái tiến trình có thể đổi ngay sau khi trả về. Đặc biệt, `sz` và `name` của tiến trình khác không được mọi đường cập nhật (như `sbrk()`/`exec()`) bảo vệ bằng `p->lock`; cách cài đặt hiện tại không cam kết snapshot nguyên tử của tất cả các trường khi tiến trình đích đang thay đổi bộ nhớ hoặc thực hiện exec đồng thời. Test quan hệ cha–con dùng pipe để giữ tiến trình con còn sống và không thay đổi các trường đó trong lúc cha truy vấn.

## 5. Chương trình kiểm thử

`user/test_procinfo.c` được đưa vào `UPROGS` trong Makefile. Chương trình thoát với mã 1 khi gặp kiểm tra thất bại và mã 0 khi mọi kiểm tra thành công.

| Nhóm kiểm tra | Điều kiện xác nhận |
|---|---|
| Tiến trình hiện tại | PID đúng, PPID dương, state=RUNNING, tên đúng, sz bằng sbrk(0) |
| Dữ liệu dư | So sánh toàn bộ struct với bản dựng đã xóa trắng, bao gồm byte đệm và đuôi name |
| Thay đổi bộ nhớ | sz tăng đúng 4096 byte sau sbrk(4096), trở lại ban đầu sau sbrk(-4096) |
| PID lỗi | PID âm, PID 0 và PID không tồn tại đều trả -1 |
| Quan hệ cha–con | Con tự xác nhận PID/PPID; cha truy vấn con khi con còn sống |
| Thu hồi tiến trình | Sau wait(), PID của con không còn được tìm thấy |
| Địa chỉ lỗi | Null, địa chỉ ngoài user-space, trang không ánh xạ, buffer cắt qua trang không ánh xạ đều trả -1 |
| Phục hồi sau lỗi | Truy vấn hợp lệ tiếp theo vẫn thành công, kích thước bộ nhớ được khôi phục |

Hai pipe đồng bộ cho phép con báo đã sẵn sàng và chờ cha cho phép kết thúc. Không giả định con luôn SLEEPING ngay sau khi gửi tín hiệu vì con có thể còn RUNNING hoặc RUNNABLE trước khi đi vào read().

## 6. Cách chạy lại

Trong Linux/WSL có make, GCC/binutils RISC-V và QEMU system RISC-V:

```sh
make clean
make
make qemu
```

Trong shell xv6:

```text
test_procinfo
trace 8388608 test_procinfo
usertests
```

Trên bản tích hợp đang kiểm tra, `SYS_procinfo=23`, nên mask trace là `1 << 23 = 8388608`. Nếu số syscall thay đổi khi tích hợp sau này, cần tính lại mask. `Ctrl-A`, sau đó `x` để thoát QEMU.

Các dấu hiệu thành công mong đợi: `=== ALL PROCINFO TESTS PASSED ===`, các dòng `syscall procinfo -> ...` trong lần chạy có trace, và `ALL TESTS PASSED` ở cuối usertests. Các dòng usertrap do bài test địa chỉ lỗi cố ý tạo ra không tự động có nghĩa là bộ test thất bại.

## 7. Kết quả kiểm chứng

Kiểm chứng ngày 04/11/2026 trong Ubuntu trên WSL, GCC RISC-V 13.3.0, GNU Make 4.3, QEMU 8.2.2; xv6 chạy 3 CPU, RAM 128 MiB, cấu hình `LAB=syscall`.

| Hạng mục | Kết quả thực tế |
|---|---|
| Biên dịch kernel và các chương trình user | Thành công; có cảnh báo linker RWX của cấu hình xv6 hiện tại |
| `test_procinfo` | Đạt toàn bộ 29 kiểm tra, in `=== ALL PROCINFO TESTS PASSED ===` |
| `trace 8388608 test_procinfo` | Đạt toàn bộ 29 kiểm tra; có log `syscall procinfo -> 0` và `-> -1` tương ứng |
| `usertests` | Không đạt toàn bộ: dừng ở `sbrkmuch`, báo `sbrk de-allocation didn't really deallocate` |
| Đối chứng commit gốc `2cc79ee` với `usertests sbrkmuch` | Tái hiện đúng lỗi trên, trước khi có bản bổ sung task 4 |

Lỗi đối chứng liên quan đến cấu hình nền: trong `kernel/vm.c`, việc xóa trắng trang mới ở `uvmalloc()` bị loại bỏ khi định nghĩa `LAB_SYSCALL`; `kernel/kalloc.c` cũng loại bỏ các lệnh điền dữ liệu khi cấp phát/giải phóng trong cấu hình này. Vì vậy bộ test chuẩn yêu cầu trang cấp phát lại được xóa trắng không đạt. Bản bổ sung task 4 không sửa cấu hình hoặc cơ chế cấp phát bộ nhớ của lab. Cần thống nhất với nhóm/giảng viên trước khi thay đổi hành vi nền này vì nó có thể phục vụ bài tập khác của lab.

Không khẳng định toàn bộ hệ điều hành đã qua usertests: các bài sau điểm thất bại chưa được chạy trong lượt kiểm thử này. Các log bàn giao gồm `procinfo-build.log`, `procinfo-validation.log` và `procinfo-baseline.log`; log baseline chứa lệnh và kết quả chạy trên mã gốc để đối chiếu độc lập.

## 8. Phạm vi bàn giao

Bản bổ sung dựa trên commit tích hợp `2cc79ee501e85373ac108abf3754b2ff67abbf9b`, thực hiện trên nhánh riêng `codex/procinfo-completion`. Thay đổi mã chức năng nằm trong `kernel/sysproc.c`; kiểm thử mở rộng nằm trong `user/test_procinfo.c`. Không gộp vào nhánh `syscall` trong phạm vi công việc này.

Tài liệu yêu cầu dùng để đối chiếu: https://docs.google.com/document/d/1aKAhr-Gm0KJMk88SbuDhZs8A5h_qVis0PlWqaqSlMI0/edit . Đây là phần nội dung để ghép vào báo cáo chung của nhóm, không phải báo cáo của toàn bộ bốn task.
