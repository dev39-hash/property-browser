# Bản dịch

> Đây là bản dịch tiếng Việt của tài liệu qpb 1.9.0. Bản tiếng Anh
> ([../../guide/translations.md](../../guide/translations.md)) là bản chuẩn; khi hai bản khác nhau thì theo bản tiếng
> Anh.

Các văn bản riêng của qpb bằng tiếng Anh, tiếng Nhật và tiếng Việt, các văn bản của riêng bạn, và việc đổi ngôn ngữ khi
đang chạy (từ 1.8). Quay lại [mục lục tài liệu](../README.md#tài-liệu).

Từ 1.8 qpb kèm bản dịch các văn bản riêng của nó cho tiếng Anh, tiếng Nhật và tiếng Việt. Chúng được nhúng trong
`qpb::core`: không có gì để deploy, và việc build không cần công cụ Qt Linguist. Hãy nạp và cài đặt chúng như bản dịch
của chính Qt:

```cpp
#include <qpb/Translations.h>

QTranslator qpbTranslator;                          // keep it as long as it is installed
if (qpb::loadTranslation(qpbTranslator, QLocale())) // the locale's UI languages: "ja-JP" finds Japanese
    app.installTranslator(&qpbTranslator);
```

`loadTranslation()` trả về false khi qpb không có bản dịch cho locale đó; các văn bản của qpb khi đó vẫn là tiếng Anh.
Khi không có bản dịch qpb nào được cài, mọi văn bản đều là văn bản tiếng Anh như ở các phiên bản trước. Ứng dụng của bạn
sở hữu translator, nên bạn kiểm soát thứ tự của các translator và lúc nào nó bị gỡ; qpb không giữ translator nào của
riêng nó.

qpb tự dịch những gì nó tự hiển thị: tiêu đề các cột, các menu ngữ cảnh, tooltip của các editor tệp và thư mục, và các
thông điệp của việc xác thực có sẵn. Mọi thứ còn lại là văn bản do bạn đưa cho qpb (tên hiển thị, tooltip, nhãn enum,
placeholder, thông điệp của các validator của bạn): hãy đưa nó đã được dịch, ví dụ bằng `tr()`. Hãy so sánh
`ValidationResult::ok`, đừng bao giờ so sánh câu chữ của thông điệp. Các văn bản mà `QObjectPropertySource` lấy từ class
info (`displayName=`, `toolTip=`) và từ các khóa `Q_ENUM` được dùng đúng như đã viết; để dịch chúng, hãy đặt lại chúng
bằng `setDisplayName()` / `setToolTip()` / attribute `options` sau `addObject()`.

**Đổi ngôn ngữ khi đang chạy**: cài và gỡ translator như bình thường. qpb cập nhật tiêu đề các cột và các editor mà nó
giữ mở; các menu ngữ cảnh được dựng khi chúng mở; các thông điệp đã hiển thị giữ nguyên ngôn ngữ của chúng. Hãy đặt lại
các văn bản của riêng bạn khi cửa sổ của bạn nhận `QEvent::LanguageChange`:

```cpp
void Window::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        model.find("general/name")->setDisplayName(tr("Name"));
        // ... tool tips, group titles, enum labels (setAttribute(qpb::Attr::Options, ...))
    }
    QWidget::changeEvent(event);
}
```

Các văn bản đến từ chính Qt bên trong editor của qpb (menu ngữ cảnh của line edit hoặc spin box, hộp thoại tệp không
native) được dịch bởi `qtbase_<lang>.qm` của Qt, do ứng dụng của bạn nạp. Qt có tiếng Nhật, nhưng không có tiếng Việt.
Các thông điệp viết bằng `qWarning()` dành cho nhà phát triển và vẫn là tiếng Anh.

**Ngôn ngữ khác, cách diễn đạt khác**: các context và văn bản gốc dưới đây ổn định trong 1.x. Hãy dịch chúng trong một
tệp `.ts` của riêng bạn (`lupdate components/qpb/src -ts myapp_qpb_de.ts` liệt kê chúng) và cài translator đó sau
translator của qpb: translator được cài sau cùng được ưu tiên. Văn bản mới có thể được thêm trong các phiên bản 1.x
sau; các context và văn bản gốc đang có không bị thay đổi.

| Context | Văn bản gốc |
|---|---|
| `qpb::PropertyModel` | `Property`, `Value` |
| `qpb::PropertyTreeView`, `qpb::PropertyFormView` | `Reset to default`, `Reset group` |
| `qpb::detail::PathEdit` | `Choose a file`, `Choose a directory` |
| `qpb::TypeRegistry` | `The value is not a number`, `At most %n characters are allowed` (số nhiều), `The value does not match the pattern %1`, `"%1" is not one of the options`, `The file %1 does not exist`, `The directory %1 does not exist` |
| `qpb::Property` | `Cannot convert the value to %1` |

Các bản dịch riêng của qpb nằm trong `translations/qpb_<lang>.ts` (các tệp `.qm` được nhúng được sinh từ chúng).
