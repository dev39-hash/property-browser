# qpb — Kế hoạch triển khai

> **Bản dịch tiếng Việt để tham khảo.** Bản chính thức là [`PLAN.md`](PLAN.md) (tiếng Anh).

> Đặc tả: [`SPEC-vi.md`](SPEC-vi.md). Ý tưởng gốc: [`brainstorm-vi.md`](brainstorm-vi.md).
> Giả định nguồn lực: **bán thời gian (~15 giờ/tuần)**, một người.

## 0. Định hướng (bản điều chỉnh)

Ba mục tiêu chi phối toàn bộ kế hoạch (SPEC §1.1):

1. **API cố định (G5):** project đã dùng qpb nâng cấp trong cùng major version **không phải sửa code**.
   → Thiết kế API *trước* khi cài đặt (API-first), bản đầu tiên cho project thật là **1.0** và khóa API ngay từ đó.
   0.x chỉ dùng nội bộ trong repo.
2. **Phân phối dạng component folder (G6):** chép `qpb/` vào `components/qpb/` của project, thêm 2 dòng CMake.
   Cập nhật = thay folder + build lại. → Cấu trúc repo lấy folder `qpb/` làm trung tâm từ ngày đầu,
   tests và examples dùng nó y như một consumer.
3. **Viết mới hoàn toàn (G7):** không wrap/fork, không chép code từ QtPropertyBrowser/QtnProperty.
   (Bước "thử lib có sẵn rồi quyết định wrap" của bản kế hoạch trước đã bị bỏ.)

Hệ quả so với bản trước: thêm ~18 giờ cho thiết kế API, kiểm thử tương thích và đóng gói;
có thêm giai đoạn **dùng thử (RC)** trong một project thật trước khi tag 1.0.

---

## 1. Tổng quan milestone

| Milestone | Tuần (ước lượng) | Kết quả kiểm chứng được                                                                 |
|-----------|------------------|------------------------------------------------------------------------------------------|
| **M0** Khung component           | 1        | Folder `qpb/` build được như một component; `tests/consumer` dùng nó bằng `add_subdirectory` |
| **M1** Thiết kế API 1.0          | 1–2      | Toàn bộ header public 1.0 được viết và review; code mẫu biên dịch được trên header       |
| **M2** Core + Model              | 2–3      | `qpb::core` cài đặt đủ, pass test kể cả `QAbstractItemModelTester`                       |
| **M3** Tree view + editor        | 4–5      | Inspector sửa được 7 kiểu, bàn phím đầy đủ → tag nội bộ `0.1.0`                          |
| **M4** Hoàn thiện & khóa API     | 6        | List mode, reset, test tương thích, gói release → `1.0.0-rc1`                            |
| **RC** Dùng thử                  | 7–8      | rc1 chạy các kịch bản tham chiếu trong ứng dụng độc lập mà không đổi API → tag `1.0.0`   |
| **M5** 1.1                       | sau 1.0  | Form view + filter (chỉ bổ sung)                                                          |
| **M6** 1.2                       | sau 1.1  | QObject adapter, serialize, Int64 (chỉ bổ sung)                                          |

M0–M4 ≈ 86 giờ (≈ 6 tuần bán thời gian). M5–M6 sẽ lập kế hoạch lại sau khi 1.0 được dùng thật.

---

## 2. Chi tiết công việc

Mỗi task có ước lượng (giờ tập trung) và tiêu chí **Done khi**.

### M0 — Khung component (≈ 8h)

**Trạng thái:** xong. Không có project thật nên M0.1 dùng các kịch bản tham chiếu trong `docs/use-cases.md`.
Option `QPB_INSTALL` (tùy chọn, SPEC §6.3) chưa thêm vì chưa có install rule nào để bật (R4).

| ID    | Task                                                                                                  | Giờ | Done khi |
|-------|-------------------------------------------------------------------------------------------------------|-----|----------|
| M0.1  | Ghi `docs/use-cases.md`: 3 kịch bản tham chiếu (không có project thật) kèm danh sách property đầy đủ | 1   | Có file; dùng làm dữ liệu cho M1 |
| M0.2  | Tạo cấu trúc repo theo SPEC §6.1: `qpb/{CMakeLists.txt,VERSION,LICENSE,CHANGELOG.md,include/qpb,src}`, CMake gốc cho dev | 2 | `cmake -S . -B build && cmake --build build` chạy |
| M0.3  | `qpb/CMakeLists.txt` tuân **toàn bộ** SPEC §6.3 (không biến global, static mặc định, `find_package` có điều kiện, không `.qrc`) | 2 | Review checklist §6.3 từng dòng |
| M0.4  | `qpbglobal.h` sinh từ `VERSION`: `QPB_VERSION*`, `QPB_VERSION_CHECK`, export macro, `QPB_DEPRECATED_X`, `QPB_DISABLE_DEPRECATED` | 1 | Test in ra `qpb::version()` |
| M0.5  | `tests/consumer/`: project CMake độc lập chép `qpb/` vào `components/qpb/` (bước copy trong CTest) rồi build app tối thiểu | 1 | CTest pass; build cả C++17 và C++20 |
| M0.6  | `.clang-format`, `.gitignore`, cập nhật README; (tùy chọn) GitHub Actions Ubuntu + Qt 6.8                | 1   | Có trong repo |

### M1 — Thiết kế API 1.0 (≈ 10h) — *API-first*

**Trạng thái:** xong. Header ở `qpb/include/qpb/`, example chỉ biên dịch ở `examples/`, phác thảo tương lai và kiểm tra
header tự đủ ở `tests/api/`, review ở [`api-review.md`](api-review.md). Example sẽ link được khi M2/M3 cài đặt xong API.

Mục tiêu: chốt hình dạng API **trước khi** có code cài đặt để phụ thuộc vào, vì sau 1.0 không còn sửa được.

| ID    | Task                                                                                                  | Giờ | Done khi |
|-------|-------------------------------------------------------------------------------------------------------|-----|----------|
| M1.1  | Viết toàn bộ header public 1.0 (chỉ khai báo + comment tài liệu): `Property`, `PropertyGroup`, builders, `Attr`, `Types`, `TypeRegistry`, `ValidationResult`, `PropertyModel`, `EditorFactory`, `PropertyDelegate`, `PropertyTreeView`, `qpb.h` | 4 | Mọi header tuân SPEC §9.3 (d-pointer, không data member public, không logic inline) |
| M1.2  | Viết trên header (chỉ biên dịch, chưa link): `examples/quickstart` (≤ 30 dòng), `examples/custom_type` (QColor), và các kịch bản tham chiếu từ M0.1 | 2 | Biên dịch được thành object file; tự đánh giá độ tiện tay |
| M1.3  | **Kiểm tra chừa chỗ cho tương lai:** phác thảo (không cài đặt) header của `PropertyFormView`, `PropertyFilterProxyModel`, `QObjectPropertySource`, serialize, `Int64` chỉ dùng API public 1.0 | 2 | Không cần thêm/sửa gì ở API 1.0; nếu cần → sửa API **bây giờ** |
| M1.4  | Review API theo checklist SPEC §9.2–9.3; ghi quyết định mới vào SPEC Phụ lục B                          | 1   | Checklist ký tên; SPEC cập nhật |
| M1.5  | Test "header tự đủ" (mỗi header public include riêng lẻ) đưa vào CTest                               | 1   | CTest pass |

Từ sau M1, thay đổi header public phải ghi lý do vào Phụ lục B (vẫn cho phép đến trước 1.0, nhưng phải có lý do).

### M2 — Core + Model (≈ 22h)

**Trạng thái:** xong. `qpb::core` đã cài đặt (`qpb/src/core/`) với 61 hàm test trong `tests/core/`
(`tst_property`, `tst_typeregistry`, `tst_propertymodel`; test model chạy dưới `QAbstractItemModelTester` chế độ Fatal).
Tiêu chí "quickstart link được" của M2.3 cần module widgets (M3); phần core được chứng minh qua `tests/consumer`, giờ dựng
model và chạy pipeline giá trị từ bản `qpb/` link static (cũng là kiểm tra link static của M2.4).

| ID    | Task                                                                                         | Giờ | Done khi |
|-------|----------------------------------------------------------------------------------------------|-----|----------|
| M2.1  | `Property` + d-pointer: dữ liệu, cờ, trạng thái hiệu lực kế thừa, `path()` (SPEC §4.2)        | 3   | Test kế thừa readOnly/enabled/visible qua 3 tầng; path đúng |
| M2.2  | `PropertyGroup`: `add`/`remove`/`find`, id trùng (SPEC §4.3)                                  | 2   | Test find lồng, remove, id trùng không tạo nút mới |
| M2.3  | Builders + `Attr::*` (SPEC §4.3–4.4)                                                          | 3   | quickstart ở M1.2 giờ link và chạy |
| M2.4  | `TypeRegistry` + 7 kiểu cơ bản: displayText, normalize, validate; đăng ký lười (SPEC §4.5, §4.7, §6.3) | 4 | Test từng kiểu; test link static không mất kiểu cơ bản |
| M2.5  | `PropertyModel` đọc: index/parent/data/flags/header, role (kể cả dải `UserRole`)             | 3   | `QAbstractItemModelTester` (Fatal) pass trên cây 3 tầng |
| M2.6  | Pipeline ghi giá trị 6 bước + `valueChanged`/`validationFailed` (SPEC §4.6)                   | 3   | `QSignalSpy`: 1 signal khi đổi, 0 khi bằng cũ/read-only; lỗi → `validationFailed` |
| M2.7  | `TreeObserver`: thêm/xóa/metadata khi model sống; `setRoot`                                   | 3   | Tester pass khi thêm/xóa lúc chạy |
| M2.8  | Batch lồng + `resetToDefault` đệ quy                                                          | 1   | `batchValueChanged` phát 1 lần ở batch ngoài cùng |

### M3 — Tree view + editor (≈ 24h)

**Trạng thái:** xong, trừ việc gắn tag nội bộ `0.1.0` (M3.8) để bạn quyết định. `qpb::widgets` đã cài đặt
(`EditorFactory` với 7 editor dựng sẵn, `PathEdit` nội bộ, `PropertyDelegate`, `PropertyTreeView`); cả 5 example đã link và chạy.
`tests/widgets` kiểm tra factory (13 hàm) và tương tác view/delegate (19 hàm: Enter/Esc/focus-out, Tab/Shift+Tab bỏ qua hàng
không sửa được, commit enum và path, bảo vệ focus khi mở dialog, checkbox, tooltip lỗi, các mode, hàng ẩn, menu reset, proxy model).
Kiểm tra thủ công bằng ảnh chụp offscreen (`QPB_SCREENSHOT_DIR`). Điểm dừng quyết định sau M3.6 không xảy ra: chỉ cần một event
filter và override `moveCursor()`.

Làm phần rủi ro cao trước: Int + String + FilePath (UX editor, focus khi mở dialog).

| ID    | Task                                                                                         | Giờ | Done khi |
|-------|----------------------------------------------------------------------------------------------|-----|----------|
| M3.1  | `EditorFactory` + tra `editorId` → `typeId` + `notifyCommit` (SPEC §5.1–5.2)                 | 2   | Test headless tạo editor theo typeId/editorId |
| M3.2  | `PropertyDelegate`: create/set/commit qua factory; Int + String                               | 3   | Edit Int → model đổi; Esc hủy; focus-out commit |
| M3.3  | `PropertyTreeView` Mode::Tree: 2 cột, group spanned, ẩn theo `IsVisibleRole`                  | 3   | `examples/inspector` hiện cây Phụ lục A |
| M3.4  | `PathEdit` file/dir + chặn FocusOut khi dialog mở + hook test (SPEC §5.3–5.4, D6)             | 4   | Dialog giả không đóng editor; chọn xong commit ngay; thử tay dialog native |
| M3.5  | Double, Enum (commit khi chọn), Bool (`CheckStateRole`, click + Space)                        | 3   | Test từng kiểu |
| M3.6  | Bàn phím: Enter, Esc, Tab/Shift+Tab bỏ qua group/read-only                                    | 4   | `QTest::keyClick` qua 5 property có group xen giữa |
| M3.7  | Lỗi validation (tooltip tại ô), elide path giữa, paint group, in đậm khi modified             | 2   | Regex sai → giữ giá trị cũ + tooltip |
| M3.8  | Kiểm tay toàn bộ inspector; sửa lỗi chặn; tag nội bộ `0.1.0`                                  | 3   | Danh sách lỗi UX; lỗi chặn đã sửa |

**Điểm dừng quyết định (cuối M3.6):** nếu Tab/FocusOut cần thay cả cơ chế edit của `QAbstractItemView`
→ cân nhắc đưa Form view (editor thường trực) lên làm view chính của 1.0. Quyết định này **phải có trước M4**
vì sau 1.0 không đổi được.

### M4 — Hoàn thiện & khóa API (≈ 22h)

**Trạng thái:** mọi việc không cần publish đã xong; bản `1.0.0-rc1` (M4.9: tag, GitHub Release, đẩy nhánh `qpb-release`)
chờ bạn quyết định. M4.1/M4.2 đã làm cùng M3. Mới trong M4: `tests/api_compat/v1_0.cpp` (M4.4), các kịch bản consumer cho
build shared, rò rỉ thiết lập của project chủ và nâng cấp tại chỗ (M4.5), `tools/check_architecture.cmake` với luật mới R6 và
`tools/count_lines.cmake` (M4.3, M4.6), CI cho Windows (MSVC) và macOS (M4.7), `tools/api_snapshot.py` với baseline
`tests/api_compat/api-1.0.txt` (M4.8), `tools/make_release.sh` và [`RELEASING.md`](RELEASING.md) (M4.9), README hướng dẫn
tích hợp/nâng cấp và review cuối trong [`api-review.md`](api-review.md) (M4.10). Các kiểm tra đã phát hiện hai lỗi thật trong
CMake của component (D31, D32).

| ID    | Task                                                                                         | Giờ | Done khi |
|-------|----------------------------------------------------------------------------------------------|-----|----------|
| M4.1  | Mode::List (D4) + nút chuyển trong inspector                                                  | 2   | Đổi Tree↔List giữ giá trị và selection |
| M4.2  | Context menu Reset to default / Reset group                                                   | 1   | Action disabled khi không modified |
| M4.3  | Hoàn thiện examples; đo S1 (≤ 30 dòng) và S2 (≤ 100 dòng) bằng script                         | 2   | Script trong CTest |
| M4.4  | `tests/api_compat/v1_0.cpp`: dùng **toàn bộ** API public 1.0 (build + chạy)                   | 3   | CTest pass; file được đóng băng từ đây |
| M4.5  | Mở rộng `tests/consumer`: static/shared, C++17/C++20, kiểm tra không rò rỉ biến global (so sánh `CMAKE_*` trước/sau `add_subdirectory`), mô phỏng "thay folder rồi build lại" | 3 | CTest pass |
| M4.6  | `tools/check_arch.sh`: core không include QtWidgets/QtGui; view không rẽ nhánh theo typeId; `src/` không bị include từ header public (R1, R2, R5) | 1 | Chạy trong CTest |
| M4.7  | Build Windows (MSVC) + macOS; header public sạch cảnh báo dưới `-Wall -Wextra -Wpedantic` / `/W4` | 3 | Sạch trên 3 OS |
| M4.8  | Tạo snapshot API gốc (`tools/api_snapshot`) để so sánh từ 1.1                                  | 2   | File snapshot trong repo |
| M4.9  | Quy trình phát hành (mục 4): `CHANGELOG.md`, zip `qpb-1.0.0-rc1.zip` chỉ chứa `qpb/`, nhánh `qpb-release` bằng `git subtree split` | 3 | Release rc1 trên GitHub |
| M4.10 | Review API lần cuối với SPEC §9; README hướng dẫn tích hợp component + chính sách nâng cấp       | 2   | Review xong |

### RC — Dùng thử trước khi khóa (1–2 tuần, song song việc khác)

**Trạng thái:** vòng 1 (`1.0.0-rc1`) phát hiện hai vấn đề hành vi, đã sửa trong `1.0.0-rc2`. Vòng 2 và 3 (`1.0.0-rc2`,
vòng 3 phủ rộng API hơn và build static/shared/Clang) không cần thay đổi API hay hành vi nào, nên RC.2 đã xong. RC.3: `1.0.0` có nội dung
của `1.0.0-rc2`, chỉ đổi `VERSION` và `CHANGELOG.md`; xem [`rc-trial.md`](rc-trial.md).
Ứng dụng thử nghiệm nằm trong `rc-trial/` và được chạy lại với mỗi release candidate.

| ID    | Task                                                                                         | Done khi |
|-------|----------------------------------------------------------------------------------------------|----------|
| RC.1  | Dựng 3 kịch bản tham chiếu (M0.1) thành một ứng dụng độc lập ngoài repo, nhúng `1.0.0-rc1` qua `components/qpb/`; thêm project thật nếu lúc đó đã có | Mọi kịch bản chạy đúng |
| RC.2  | Ghi mọi chỗ "khó chịu" với API. Nếu phải đổi API → sửa, phát `rc2`, lặp lại RC                  | Một vòng RC không cần đổi API |
| RC.3  | Tag `1.0.0` (cùng nội dung RC cuối, chỉ đổi `VERSION`)                                          | Tag + release zip |

### M5 — 1.1 (chỉ bổ sung, lập kế hoạch lại sau 1.0)

M5.1 `PropertyFormView` (SPEC §5.6) · M5.2 đồng bộ hai chiều form ↔ model · M5.3 `PropertyFilterProxyModel` + ô search ·
M5.4 attribute `multiline` · M5.5 `tests/api_compat/v1_1.cpp` + diff API snapshot (chỉ được thêm) · M5.6 release 1.1.0.

**Trạng thái:** M5.1–M5.5 xong. `PropertyFormView` (`qpb/src/widgets/PropertyFormView.cpp`, 17 test trong
`tests/widgets/tst_propertyformview.cpp`), `PropertyFilterProxyModel` trong `qpb::core` (7 test), attribute `multiline`
(tree view và form), `tests/api_compat/v1_1.cpp` và snapshot `api-1.1.txt` (chứa trọn `api-1.0.txt`, file này vẫn được
kiểm tra). Ô search là `QLineEdit` của ứng dụng nối với `setFilterFixedString()`, minh họa trong `examples/form_view`.
Quyết định D36–D39 trong SPEC. M5.6: đã phát hành `1.1.0`.

### M6 — 1.2 (chỉ bổ sung)

M6.1 `QObjectPropertySource` · M6.2 serialize JSON/`QSettings` · M6.3 `Types::Int64` · M6.4 example `QUndoStack` ·
M6.5 `v1_2.cpp` + diff snapshot · M6.6 release 1.2.0.

**Trạng thái:** M6.1–M6.5 xong. `QObjectPropertySource` (SPEC §4.9, 8 test), `qpb::serialization` (§4.8, 10 test gồm
`QSettings` INI và native), `Types::Int64` với spin box 64-bit nội bộ, `examples/object_editor` (QObject source +
`QUndoStack` + JSON), `tests/api_compat/v1_2.cpp` và `api-1.2.txt` (chứa trọn 1.1). Bước kiểm tra biên dịch phát hiện hàm
tự do trong `qpb` làm hỏng lời gọi không kèm namespace của ứng dụng qua ADL, nên dùng namespace lồng (D40). Quyết định
D40–D42. M6.6: đã phát hành `1.2.0`, sau vòng thử RC 4 (docs/rc-trial.md) phát hiện và sửa F7.

---

## 3. Phụ thuộc giữa các task

```text
M0.2 ─► M0.3 ─► M0.4 ─► M0.5
M0.* ─► M1.1 ─► M1.2, M1.3 ─► M1.4 ─► M1.5
M1.* ─► M2.1 ─► M2.2 ─► M2.3
        M2.4 ─► M2.5 ─► M2.6 ─► M2.7 ─► M2.8
M2.4 ─► M3.1 ─► M3.2 ─► M3.3 ─► M3.4, M3.5 ─► M3.6 ─► M3.7 ─► M3.8
M3.* ─► M4.* ─► RC ─► 1.0.0 ─► M5 ─► M6
```

---

## 4. Quy trình phát hành (mọi bản 1.x)

Hướng dẫn chi tiết và công cụ: [`RELEASING.md`](RELEASING.md), `tools/make_release.sh`.

1. Cập nhật `qpb/VERSION` và mục mới trong `qpb/CHANGELOG.md` (Added / Changed / Deprecated / Fixed / **Upgrade notes**).
2. CI xanh: unit test, `api_compat/*` của **mọi** bản trước, `tests/consumer`, `check_arch`, diff API snapshot chỉ có thêm.
3. Thêm `tests/api_compat/v<x_y>.cpp` cho bản mới (bản minor) và cập nhật snapshot.
4. Tag `vX.Y.Z` trên `main`.
5. Tạo artifact: `qpb-X.Y.Z.zip` (chỉ folder `qpb/`) đính kèm GitHub Release; cập nhật nhánh `qpb-release`
   (`git subtree split --prefix qpb -b qpb-release`) cho consumer dùng `git subtree pull`.

**Phía consumer khi nâng cấp:** thay `components/qpb/` → build lại → đọc *Upgrade notes*. Với 1.x mong đợi là "không cần làm gì".

---

## 5. Quy trình làm việc

- **Nhánh:** mỗi task/nhóm task một nhánh `feat/M2.4-type-registry`, PR vào `main`, squash merge.
- **Definition of Done cho mọi PR:** build sạch cảnh báo; test mới cho hành vi mới; `ctest` pass (widgets `offscreen`);
  không vi phạm R1–R5 (SPEC §3); public API mới có nơi dùng.
- **PR chạm `qpb/include/`:** phải có dòng "API change: none / additive / breaking" trong mô tả.
  Sau 1.0, "breaking" bị từ chối trừ khi đang làm 2.0.
- **Commit:** Conventional Commits (`feat(core): ...`, `fix(widgets): ...`).
- **Không chép code** từ QtPropertyBrowser/QtnProperty hay nguồn có license khác (G7); chỉ tham khảo hành vi UX.

---

## 6. Rủi ro & phương án cắt phạm vi

| Rủi ro                                        | Dấu hiệu                                       | Phản ứng                                                                    |
|-----------------------------------------------|------------------------------------------------|-----------------------------------------------------------------------------|
| API 1.0 sai mà đã khóa                        | Trong RC phải thêm workaround ở project thật   | Kéo dài RC; không tag 1.0 khi còn nghi ngờ. Sau 1.0: chỉ thêm API mới + deprecate cái cũ |
| Chậm tiến độ                                  | Hết tuần 5 chưa xong M3.6                       | Cắt khỏi 1.0 (thêm lại ở 1.x là *bổ sung*, không phá API): DirPath → Tab navigation → build shared. **Không** cắt M1, M4.4–M4.6 |
| UX editor cần hack sâu                        | M3.4/M3.6 vượt 2× ước lượng                    | Điểm dừng cuối M3.6                                                         |
| Consumer có cấu hình CMake lạ làm hỏng build  | RC.1 lỗi CMake                                  | Thêm case vào `tests/consumer`, sửa `qpb/CMakeLists.txt`                   |
| `TreeObserver` gây lỗi index                   | `QAbstractItemModelTester` fail                | 1.0 chỉ cho đổi cấu trúc qua `setRoot`; thêm/xóa lúc chạy là bổ sung ở 1.1 (API `add/remove` đã có sẵn, chỉ ghi rõ hạn chế) |
| Dialog native khác nhau giữa OS               | Editor đóng khi dialog mở trên Windows/macOS   | Fallback: đóng editor trước khi mở dialog, kết quả ghi qua `model->setData` |

---

## 7. Quyết định đã chốt / còn mở

| Câu hỏi                                   | Trạng thái                                     |
|-------------------------------------------|------------------------------------------------|
| C++17 hay C++20?                          | **Chốt: C++17**                                |
| Qt tối thiểu?                             | **Chốt: 6.5**                                  |
| Namespace / tên target / prefix include   | **Chốt: `qpb`, `qpb::core`, `qpb::widgets`, `<qpb/...>`** |
| Build system của các project dùng qpb     | **Chốt: chỉ CMake**                            |
| Ngôn ngữ code và tài liệu                  | **Chốt: tiếng Anh** (bản `-vi` để tham khảo)   |
| Cách đồng bộ folder components            | Mở — mặc định chép tay từ zip                  |
| License                                   | Mở — giả định MIT                              |
| "Custom property table" nghĩa là gì?      | **Chốt:** người dùng tự dựng property table bằng API public của thư viện (G1, G3); không thêm khái niệm mới |
| Nguồn dữ liệu chính?                      | Mở — giả định builder tường minh; cần chốt trước M1.3 |
