# AOV Zygisk v2.6.0 — Liên Quân Mobile Mod Module

### First Frost UI — revision 2

Gói giao diện hiện tại: `aov_zygisk_v2.6.0_first_frost_r2.zip`.
Nút vàng ẩn toàn bộ menu, nút mở AOV lớn hơn và hỗ trợ kéo thả.
Tab Status bổ sung thông tin hiệu năng, thời gian chạy, lựa chọn hiện tại và bản lưu.
Trong **Settings → Lưu cấu hình**, bật để lưu ngay và tự cập nhật các thay đổi;
lần mở game tiếp theo sẽ khôi phục các tùy chọn đó. Tắt để ngừng khôi phục từ lần
mở game sau, không thay đổi các tính năng đang bật trong phiên hiện tại.

[Ảnh giao diện, cơ chế lưu và kiểm chứng](docs/first-frost-ui.md).

**Target:** `com.garena.game.kgvn` (Liên Quân Mobile / Arena of Valor) v1.63.1.14  
**Engine:** Unity IL2CPP — `libmain.so` + `libunity.so` + `libil2cpp.so`  
**Root:** KernelSU Next / APatch / Magisk (yêu cầu Zygisk / ZygiskNext)  
**Kiến trúc:** `arm64-v8a` only  
**Gói cài đặt:** `aov_zygisk_v2.6.0.zip`

---

## 🌟 Tính Năng Mới & Nâng Cấp Giao Diện (v2.6.0)

### 1. Giao Diện Điều Khiển Cao Cấp & Tiếng Việt Đầy Đủ
- **Nút Tròn Nổi Kéo Thả (Draggable Floating Button):**
  - Thiết kế nút tròn phong cách Neon Cyan hiện đại, viền phát sáng mượt mà.
  - Tự do chạm giữ và kéo quanh 4 cạnh màn hình, tự động lưu lại vị trí.
  - Phân biệt thông minh giữa chạm nhả (Tap để mở menu) và kéo (Drag để di chuyển vị trí).
- **Chế Độ Ẩn Hoàn Toàn (Tàng Hình Tránh Lộ):**
  - Tích hợp nút **"Ẩn Nút"** trên thanh tiêu đề menu.
  - Nút tròn sẽ ẩn 100% khỏi màn hình (rất thích hợp cho người dùng quay video hoặc phát trực tiếp).
  - Hệ thống tự động ghi nhớ vị trí nút: Chỉ cần **chạm nhẹ lại vào đúng vị trí cũ** là menu sẽ mở ra!
- **Menu To Rộng & Mạch Lạc:**
  - Menu được thiết kế lớn hơn, thoáng hơn, hỗ trợ cuộn mượt mà.
  - Toàn bộ tùy chọn sử dụng nút gạt công tắc chuyển đổi (Toggle Switch) phong cách iOS/Material.
  - Hiển thị Tiếng Việt có dấu đầy đủ 100% chuẩn UTF-8 qua font hệ thống Roboto/DroidSans.

---

### 2. Trọn Bộ Tính Năng In-Game (1.63.1.14)
| Danh Mục | Tính Năng Chi Tiết |
| :--- | :--- |
| **Bản Đồ** | • **Hack Bản Đồ V2:** Can thiệp `LVActorLinker.SetVisible` hiển thị toàn bộ tướng địch trong tối/bụi.<br>• **Hiện Tên & Avatar:** Hiển thị vị trí và tên tướng địch trên minimap.<br>• **Thời Gian Hồi Chiêu:** Đếm giây hồi chiêu trên đầu tướng (Phép phụ trợ: **SỐ VÀNG** \| Chiêu 1, 2, 3: **SỐ TRẮNG**).<br>• **Camera Xa:** Kéo tầm nhìn bao quát toàn bộ bản đồ từ 0 - 30. |
| **Ngắm & Tự Động** | • **Aimbot Định Hướng:** Hỗ trợ khóa mục tiêu tự động cho Chiêu 1, Chiêu 2, Chiêu Cuối.<br>• **Chế Độ Ngắm:** Ưu tiên % máu thấp nhất, máu tuyệt đối thấp nhất, gần nhất, gần tâm ngắm.<br>• **Dự Đoán Đón Đầu (Lead):** Tính toán vận tốc di chuyển kẻ địch để ngắm trước.<br>• **Tia Ngắm Elsu:** Vẽ đường ngắm trực quan.<br>• **Auto Bộc Phá:** Tự động kích hoạt khi máu địch <= 15% lượng máu đã mất trong phạm vi 5m.<br>• **Auto Trừng Trị:** Tự động kết liễu Tà Thần Caesar, Rồng và Bùa Xanh/Đỏ chuẩn xác. |
| **Trang Phục** | • **Mở Khóa Toàn Bộ Skin:** 14 hook IL2CPP chuyển hướng trang phục được chọn.<br>• **Thông Báo Hạ Gục:** Tùy biến bảng vàng thông báo hạ gục (broadcastID 0 - 61).<br>• **Skin Nút Bấm:** Tùy biến bộ phím kỹ năng cá nhân hóa (PersonalButtonID 0 - 73). |
| **Mở Khóa FPS** | • **120 FPS Siêu Mượt:** Vượt qua giới hạn cấu hình với 10 hooks GameSettings. |
| **Bảo Vệ** | • **Anti-Cheat 5 Tầng:** Cách ly Namespace, ẩn thư viện khỏi maps, đổi tên thread qua SVC #0, giả mạo thuộc tính hệ thống. |

---

## 🛡️ Hệ Thống 5 Tầng Anti-Cheat (Bypass Tersafe / ACE)
1. **Tầng 1 (post-fs-data.sh):** Khóa quyền đọc `/sys/fs/selinux/enforce`, resetprop các cờ boot, chặn domain báo cáo qua DNS AdGuard.
2. **Tầng 1b (service.sh):** Mask các build property (`ro.build.tags=release-keys`, `ro.debuggable=0`, xóa `ro.kernelsu.version`).
3. **Tầng 2 (Zygisk Native):** `FORCE_DENYLIST_UNMOUNT` tháo dỡ toàn bộ mount namespace khỏi tiến trình game.
4. **Tầng 3 (Maps Hider):** Ẩn các trang bộ nhớ của module khỏi `/proc/self/maps`.
5. **Tầng 4 (SVC #0 Rename):** Đổi tên thread mod thành `UnityGfxDevice` qua Direct Linux Syscall (SVC #0) bypass kiểm tra `/proc/self/task`.
6. **Tầng 5 (Prop Spoofer):** Hook `__system_property_get` trả về giá trị máy chuẩn trước khi Tersafe quét.

---

## 📦 Cài Đặt & Sử Dụng

1. Tải file `aov_zygisk_v2.6.0.zip`.
2. Mở trình quản lý **KernelSU Next**, **APatch** hoặc **Magisk**.
3. Cài đặt file zip và khởi động lại thiết bị (yêu cầu bật module **ZygiskNext** nếu dùng KernelSU/APatch).
4. Khởi chạy Liên Quân Mobile:
   - Một **nút tròn nổi màu xanh neon** sẽ xuất hiện ở góc trên bên trái màn hình.
   - Chạm vào nút tròn để **Mở / Đóng** bảng điều khiển.
   - Nhấn giữ và kéo nút tròn để **thay đổi vị trí** tùy ý trên màn hình.
   - Bấm **"Ẩn Nút"** trên menu để giấu hoàn toàn nút (chạm lại đúng vị trí cũ để mở lại).
