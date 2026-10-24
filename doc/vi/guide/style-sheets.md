# Style sheet

> Đây là bản dịch tiếng Việt của tài liệu qpb 1.9.0. Bản tiếng Anh
> ([../../guide/style-sheets.md](../../guide/style-sheets.md)) là bản chuẩn; khi hai bản khác nhau thì theo bản tiếng
> Anh.

Tạo giao diện cho các view và editor bằng style sheet của Qt (từ 1.6). Quay lại
[mục lục tài liệu](../README.md#tài-liệu).

Các selector chuẩn bao phủ các view và mọi editor: `qpb--PropertyTreeView`, `qpb--PropertyFormView`, `::item`
(`:hover`, `:selected`, `:alternate`), `QHeaderView::section`, và các editor theo lớp Qt của chúng
(`qpb--PropertyTreeView QLineEdit` cho các editor trong ô, `qpb--PropertyFormView QLineEdit` trong form). Với những gì
qpb tự vẽ hoặc tự dựng:

**Tree view**: các property màu, đặt được bằng `qproperty-`. Giá trị không hợp lệ giữ mặc định; một quy tắc `::item`
khớp có `color` hoặc `background` được ưu tiên hơn. Font là cố định (tên nhóm và tên đã sửa vẫn in đậm).

| Property | Mặc định |
|---|---|
| `groupBackground` (brush) | màu Button của palette |
| `groupForeground` | màu văn bản của item |
| `modifiedForeground` | màu văn bản của item |
| `readOnlyForeground` | PlaceholderText của palette |

Các giá trị tương tự có sẵn từ code: `setGroupBackground()` và các setter khác của `PropertyTreeView`. Một màu không hợp
lệ hoặc `Qt::NoBrush` chọn mặc định.

**Form view và các path editor**: các property động trên widget của chúng.

| Widget | Selector |
|---|---|
| section, nút tiêu đề, phần thân của một nhóm | `QWidget[qpbPart="group"]`, `QToolButton[qpbPart="groupTitle"]`, `QWidget[qpbPart="groupBody"]` |
| nhãn của một property | `QLabel[qpbPart="label"]`, và `[qpbModified="true"]` khi đã sửa |
| văn bản read-only của một kiểu không có editor | `QLabel[qpbPart="value"]` |
| nút "..." của các editor tệp và thư mục | `QToolButton[qpbPart="browse"]` |

```css
qpb--PropertyTreeView { qproperty-groupBackground: #2c3038; qproperty-modifiedForeground: #e87c00; }
QToolButton[qpbPart="groupTitle"], QToolButton[qpbPart="groupTitle"]:checked {
    background: #2c3038; color: #e87c00; border: none; border-bottom: 1px solid #e87c00;
}
QLabel[qpbPart="label"][qpbModified="true"] { color: #e87c00; }
```

## Màu của model trong form view

`PropertyFormView::showModelForeground` (từ 1.9, mặc định tắt) làm cho nhãn và tiêu đề section hiển thị màu
`Qt::ForegroundRole` của model (của ô tên, nếu không thì của ô giá trị). Một style sheet có thể bật nó:

```css
qpb--PropertyFormView { qproperty-showModelForeground: true; }
```

Giá trị tương tự có sẵn từ code: `setShowModelForeground()`. Thứ tự ưu tiên khác với tree view, nơi một quy tắc `::item`
khớp được ưu tiên hơn màu của model:

- Màu của model được ưu tiên hơn một `color` trong style sheet của ứng dụng cho `QLabel[qpbPart="label"]` và
  `QToolButton[qpbPart="groupTitle"]`, kể cả quy tắc `[qpbModified="true"]`. Form đặt màu làm style sheet riêng của
  widget, thứ mà style sheet của ứng dụng không thể ghi đè. Để có màu khác, hãy trả về một brush khác từ model hoặc từ
  một proxy thay vì viết một quy tắc.
- Khi không có màu từ model (không được cung cấp, hoặc model thôi không cung cấp nữa) widget theo style và style sheet
  của ứng dụng như thường lệ, nên quy tắc `[qpbModified="true"]` lại được áp dụng.
- Nhãn và tiêu đề đã sửa vẫn in đậm. Form không đặt màu nào trên một nhãn disabled (quy tắc của nó là `QLabel:enabled`),
  nên một nhãn disabled giữ màu mà nó có khi không có property này: màu disabled của style, hoặc của style sheet ứng
  dụng.
- Khi property tắt, form không đặt gì cả, bất kể model trả về gì.

## Ghi chú

- Tiêu đề section là một tool button có thể check (được check khi mở rộng), nên hãy bao phủ `:checked` nếu style sheet
  của bạn tạo kiểu cho các tool button đã check.
- Các giá trị `qproperty-` vẫn còn khi một style sheet bị gỡ; hãy đặt chúng về các giá trị không hợp lệ khi chuyển sang
  một theme không có chúng.
- Một quy tắc `::branch` thay thế các mũi tên mở rộng của tree và khi đó cần `image:` cho `:closed` và `:open`.
- Nhãn và tiêu đề in đậm vẫn in đậm khi một style sheet tạo lại kiểu cho `QLabel` hoặc `QToolButton`: form view đặt lại
  độ đậm sau khi style sheet được áp dụng.
- Chỉ các tên và giá trị property này là một phần của API, không phải tên lớp của các widget nội bộ.
