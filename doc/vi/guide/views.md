# View

> Đây là bản dịch tiếng Việt của tài liệu qpb 1.9.0. Bản tiếng Anh ([../../guide/views.md](../../guide/views.md)) là bản
> chuẩn; khi hai bản khác nhau thì theo bản tiếng Anh.

Tree view, form view, filter proxy cho ô tìm kiếm, delegate cho các view của riêng bạn, bàn phím, và luồng (thread).
Quay lại [mục lục tài liệu](../README.md#tài-liệu).

Cả hai view đều hiển thị một `PropertyModel`, hoặc một proxy model có nguồn là một `PropertyModel`. Không view nào sở
hữu model. Các chỉnh sửa đi qua model (`setData`), nên việc xác thực và các signal là như nhau dù người dùng làm việc
trong view nào. Với code của bạn, cả hai trông giống nhau: bạn dựng cây, tạo một model, và gọi `setModel()`.

## Tree view

`PropertyTreeView` là một `QTreeView` với hai cột, tên và giá trị.

- **Chế độ (Modes).** `Mode::Tree` (mặc định) hiển thị các nhóm có thể thu gọn, lúc đầu được mở rộng. `Mode::List` hiển
  thị cùng model đó ở dạng phẳng, với các nhóm là tiêu đề section không thu gọn được. `setMode()` chuyển đổi bất kỳ lúc
  nào và giữ nguyên model, các giá trị và vùng chọn hiện tại.
- **Diện mạo (Looks).** Các hàng nhóm in đậm trên màu Button của palette; tên của các property đã sửa in đậm; các giá
  trị read-only bị làm mờ; các property hidden là các hàng bị ẩn; các hàng xen kẽ màu. Các giá trị dài được rút gọn
  (elide) ở giữa. Màu có thể được đặt từ style sheet; xem [Style sheet](style-sheets.md).
- **Cột tên.** Nó vừa khít nội dung của mình (tối đa 60% của view) cho đến khi bạn đặt độ rộng bằng
  `setNameColumnWidth()` hoặc người dùng kéo header.
- **Menu ngữ cảnh.** "Reset to default" trên một property (bị disable khi nó chưa sửa, là read-only hoặc là disabled)
  và "Reset group" trên một nhóm (được bật khi có ít nhất một property trong đó có thể đặt lại).
- **Chỉnh sửa.** Editor mở khi item hiện tại thay đổi, khi click vào một item đang được chọn, hoặc bằng phím chỉnh sửa
  của nền tảng. Các giá trị được commit qua model; nếu model từ chối một giá trị, editor đóng, giá trị cũ vẫn còn và
  thông điệp hiện dưới dạng tooltip tại ô.

## Form view

`PropertyFormView` là một `QScrollArea` (từ 1.1) hiển thị một form: một nhãn và một editor cho mỗi property, và mỗi nhóm
là một section có thể thu gọn với một nút tiêu đề và một phần thân thụt vào.

- Các editor luôn mở; một kiểu không có editor được hiển thị dưới dạng văn bản read-only chọn được. `editor(path)` trả
  về widget của một property (nhãn văn bản với một kiểu không có editor), hoặc `nullptr` với nhóm, đường dẫn không tồn tại
  và các property bị một proxy lọc ra.
- Trạng thái mở rộng hay thu gọn được giữ theo đường dẫn, kể cả khi form được dựng lại: `isExpanded(groupPath)` (mặc
  định là true) và `setExpanded()`. Trạng thái có thể được đặt cho một nhóm chưa tồn tại.
- Form đi theo model: giá trị, tên hiển thị, tooltip, tính hiển thị, trạng thái read-only và enabled, attribute, và các
  property được thêm hoặc xóa. Nhãn của các property đã sửa in đậm. Các property hidden ẩn nhãn và editor của chúng, hoặc
  cả section với một nhóm; một property disabled có nhãn và editor disabled, một property read-only có editor disabled
  và nhãn bình thường. Một editor chỉ được ghi lại khi giá trị của model thay đổi (hoặc khi các attribute của nó được
  áp dụng lại), nên văn bản đang gõ được giữ qua các cập nhật khác như một tooltip hay màu mới.
- `showModelForeground` (một `Q_PROPERTY`, từ 1.9, mặc định tắt): nhãn và tiêu đề section hiển thị màu
  `Qt::ForegroundRole` của model, của ô tên hoặc nếu không thì của ô giá trị, kể cả khi một section đang thu gọn. Một
  `QColor` hoặc một `QBrush` có một màu duy nhất được chấp nhận; brush gradient và texture, `Qt::NoBrush` và các giá
  trị khác nghĩa là không có màu. Màu đi theo `dataChanged()` mà không cần dựng lại, được ưu tiên hơn style sheet của
  ứng dụng, và không đụng đến độ đậm của nhãn và tiêu đề đã sửa; một nhãn disabled không nhận màu của model và giữ
  màu mà nó có khi không có property này (màu disabled của style, hoặc của style sheet ứng dụng). Khi property tắt,
  các màu của model bị bỏ qua.
  Xem [Style sheet](style-sheets.md#màu-của-model-trong-form-view).
- Các thay đổi cấu trúc trong model (hàng được thêm, xóa hoặc di chuyển, một lần reset model, một thay đổi layout) dựng
  lại form một lần, khi quyền điều khiển trở về event loop, nên một bộ lọc thay đổi nhiều hàng cùng lúc chỉ tốn một lần
  dựng lại. Editor đang có focus được commit trước và nhận lại focus sau đó. `editor(path)`, `setExpanded()`, constructor
  và `setModel()` không chờ: chúng đưa form về trạng thái cập nhật ngay lập tức.
- Menu ngữ cảnh của một nhãn cung cấp "Reset to default"; menu của một tiêu đề section cung cấp "Reset group". Chúng được
  bật trong cùng các điều kiện như ở tree view.

## Tìm kiếm: filter proxy

`PropertyFilterProxyModel` (từ 1.1) lọc theo tên hiển thị, không phân biệt hoa thường. Dùng nó giữa model và một trong
hai view:

```cpp
qpb::PropertyFilterProxyModel proxy;
proxy.setSourceModel(&model);
QObject::connect(searchEdit, &QLineEdit::textChanged, &proxy, &QSortFilterProxyModel::setFilterFixedString);
view.setModel(&proxy);
```

Một nhóm được hiển thị khi một trong các phần tử con cháu của nó khớp, và hiển thị cùng toàn bộ nội dung khi tên của chính
nó khớp. Các property hidden không bao giờ khớp. Hãy đặt văn bản bằng các hàm `QSortFilterProxyModel` thông thường như
`setFilterFixedString()`. Khi bạn tra cứu property sau khi lọc, hãy ánh xạ index trở lại model nguồn bằng
`mapToSource()`.

## View của riêng bạn

`PropertyTreeView` cài một `PropertyDelegate`. Để có cách chỉnh sửa tương tự trong một `QAbstractItemView` khác đang hiển
thị một `PropertyModel` (hoặc một proxy của nó), hãy đặt delegate trên đó:

```cpp
anyView->setItemDelegate(new qpb::PropertyDelegate(anyView));
```

Delegate tạo editor bằng `EditorFactory`, vẽ các giá trị (check box, đã sửa, read-only) và xử lý các phím bên dưới. Các
giá trị bị từ chối giữ nguyên giá trị cũ và hiển thị thông điệp dưới dạng tooltip. Một editor đang mở đi theo model: khi
các attribute của property thay đổi (một khoảng, một hậu tố, ...) chúng được áp dụng lại cho editor, và editor chỉ được
ghi lại khi giá trị của model thay đổi (hoặc sau khi các attribute của nó được áp dụng lại), nên văn bản đang gõ được
giữ qua các cập nhật khác như một tooltip mới. Điều này đúng trong mọi view dùng delegate.

## Bàn phím

| Phím | Tree view (và delegate) | Form view |
|---|---|---|
| Enter | Commit và đóng editor. Trong editor chuỗi nhiều dòng, Enter chèn một dấu ngắt dòng và Ctrl+Enter commit | Commit. Trong editor chuỗi nhiều dòng, Enter chèn một dấu ngắt dòng và Ctrl+Enter commit |
| Esc | Hủy việc chỉnh sửa | Đặt giá trị của model trở lại vào editor |
| Tab / Shift+Tab | Commit và mở editor của giá trị chỉnh sửa được kế tiếp / liền trước, bỏ qua nhóm, hàng read-only và hidden, và check box | Chuỗi focus bình thường của form |
| Space | Bật/tắt một check box | Bật/tắt một check box |
| Mất focus | Commit (không khi một editor đang hiển thị hộp thoại hoặc một popup đang hoạt động) | Commit (không khi một editor đang hiển thị hộp thoại hoặc một popup đang hoạt động) |

Trong tree view, các check box mặc định không nằm trong chuỗi Tab; các phím mũi tên đến được chúng. Hãy gọi
`setTabStopsOnCheckBoxes(true)` (từ 1.5) để Tab và Shift+Tab dừng trên các check box mà người dùng có thể đổi: không có
editor nào mở ở đó, Space bật/tắt giá trị, và lần Tab kế tiếp đi tiếp. Việc rời khỏi view ở một trong hai đầu hoạt động
như trước. Form view không cần tùy chọn như vậy: các check box của nó là các widget bình thường trong chuỗi focus.

Với attribute `commitOnStep` (từ 1.9), mỗi bước của một spin box Int, Int64 hoặc Double (nút mũi tên, phím mũi tên, con
lăn, Page Up / Down) được commit ngay và editor của tree view vẫn mở; văn bản gõ vào vẫn chờ Enter hoặc mất focus.

Form view commit một editor khi mất focus, khi Enter (Ctrl+Enter trong editor nhiều dòng), và ngay lập tức với các check
box, combo box, path editor và editor tùy biến có gọi `EditorFactory::notifyCommit()`. Một giá trị không thay đổi thì
không được ghi.

## Luồng (thread)

qpb là một thư viện của GUI thread. Chỉ dùng model, các view và các registry (`TypeRegistry`, `EditorFactory`) từ GUI
thread.
