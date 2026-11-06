# Sửa lỗi sbrkmuch khi chạy usertests

Ngày kiểm chứng: 06/11/2026. Nhánh sửa: `feat/integration`; commit nền: `2cc79ee501e85373ac108abf3754b2ff67abbf9b`.

## Hiện tượng và nguyên nhân

Trước khi sửa, `usertests` dừng ở bài `sbrkmuch` với thông báo:

```text
sbrkmuch: sbrk de-allocation didn't really deallocate
FAILED
SOME TESTS FAILED
```

Bài kiểm tra ghi dữ liệu vào trang cuối, thu nhỏ không gian địa chỉ bằng sbrk âm, rồi cấp phát lại trang đó. Trang mới phải được xóa trắng trước khi trả cho chương trình. Trong cấu hình `LAB=syscall`, `uvmalloc()` tại `kernel/vm.c` bỏ qua `memset(mem, 0, sz)` do điều kiện `#ifndef LAB_SYSCALL`. Đồng thời các lệnh điền dữ liệu gỡ lỗi trong `kernel/kalloc.c` cũng bị tắt, nên trang có thể còn giữ byte cũ. Việc trang được trả cho allocator không đồng nghĩa nội dung vật lý của trang đã bị xóa.

## Thay đổi

Bỏ điều kiện LAB_SYSCALL quanh lệnh xóa trắng trong `uvmalloc()`. Mọi trang chuẩn bị ánh xạ vào user-space được xóa trước khi `mappages()` đưa trang đó vào bảng trang. Không thay đổi bài usertests, điều kiện đạt của sbrkmuch, thuật toán cấp phát hay giải phóng trang.

```c
// Newly mapped user pages must not expose data from their previous owner.
memset(mem, 0, sz);
```

Cách sửa này khôi phục bảo đảm dữ liệu ban đầu của trang mới, đồng thời ngăn chương trình đọc dữ liệu cũ của tiến trình khác qua trang được tái sử dụng. Không cần thay đổi kalloc/kfree vì chúng phục vụ cả cấp phát nội bộ kernel; điểm cần bảo đảm dữ liệu user-space là uvmalloc.

## Kiểm chứng

Môi trường Linux tạm: Debian 12 trong WSL2, GCC RISC-V 12.2.0, QEMU 7.2.22, xv6 3 CPU/128 MiB, `LAB=syscall`. Biên dịch sạch bằng `make clean`, `make`, sau đó `make qemu`.

Các lệnh trong shell xv6:

```text
usertests sbrkmuch
test_procinfo
trace 8388608 test_procinfo
usertests
```

| Lệnh | Kết quả |
|---|---|
| `usertests sbrkmuch` | Đạt, `ALL TESTS PASSED` |
| `test_procinfo` | Đạt 5 nhóm test gốc |
| `trace 8388608 test_procinfo` | Đạt; có kết quả syscall 0 và -1 đúng các trường hợp |
| `usertests` | Đạt toàn bộ test nhanh và test chậm; kết thúc bằng `ALL TESTS PASSED` |

Lượt chạy toàn bộ usertests mất khoảng 463 giây. Các thông báo usertrap, balloc out of blocks và ialloc no inodes trong log thuộc các tình huống lỗi được bài kiểm tra cố ý tạo ra; các bài tương ứng đều đạt. Đây là kết quả sau bản sửa, thay thế kết luận không đạt sbrkmuch ở các báo cáo lịch sử trước đó cho phiên bản nhánh đã sửa.

Log: [sbrkmuch-fix-validation.txt](testing/sbrkmuch-fix-validation.txt).

## Ảnh hưởng tới bài attack của bộ lab gốc

Repo còn chứa bài `attack`/`attacktest`, không thuộc bốn hạng mục xargs/tree/trace/procinfo trong tài liệu phân công. Bài attack gốc cố ý dựa vào việc các trang không được xóa trắng để khai thác dữ liệu bí mật của tiến trình trước. Sau bản sửa này, cách khai thác đó không còn áp dụng cho những trang được cấp qua uvmalloc. Không khẳng định bộ chấm `make grade` của lab syscall gốc sẽ đạt mục attack; đây là thay đổi có chủ đích để khôi phục an toàn bộ nhớ và đáp ứng usertests. `user/attack.c` trong repo hiện vẫn là khung bài chưa cài đặt.
