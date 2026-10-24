# Giá trị và xác thực

> Đây là bản dịch tiếng Việt của tài liệu qpb 1.9.0. Bản tiếng Anh
> ([../../guide/values-and-validation.md](../../guide/values-and-validation.md)) là bản chuẩn; khi hai bản khác nhau
> thì theo bản tiếng Anh.

Điều gì xảy ra khi một giá trị thay đổi, qpb thực hiện những kiểm tra nào, cách thêm kiểm tra của riêng bạn, và cách giá
trị mặc định cùng việc đặt lại hoạt động. Quay lại [mục lục tài liệu](../README.md#tài-liệu).

## Pipeline xử lý giá trị

Mọi thay đổi, từ một view hay từ code, đều đi qua cùng các bước:

1. Giá trị được chuyển đổi sang kiểu lưu trữ của kiểu (ví dụ một chuỗi gõ vào spin box trở thành một `int`). Nếu việc
   chuyển đổi thất bại, giá trị bị từ chối.
2. Nó được chuẩn hóa: các giá trị `Int`, `Int64` và `Double` bị clamp vào `Attr::Minimum` / `Attr::Maximum`, và các giá
   trị `Double` được làm tròn theo `Attr::Decimals` (trừ khi `Attr::RoundToDecimals` là `false`, xem bên dưới).
3. Việc xác thực riêng của kiểu chạy (xem bảng bên dưới).
4. Validator của property chạy, nếu nó có.
5. Một giá trị bằng giá trị hiện tại thì không thay đổi gì và không phát ra gì.
6. Nếu không, giá trị được lưu và model phát `valueChanged(path, newValue, oldValue)`.

Một giá trị bị từ chối (ở bước 1, 3 hoặc 4) giữ nguyên giá trị cũ và làm model phát `validationFailed(path,
rejectedValue, message)`; các view hiển thị thông điệp dưới dạng tooltip. Các con số bị clamp thay vì bị từ chối, như
một spin box vẫn làm, để một giá trị đặt từ code hoạt động giống một giá trị do người dùng gõ.

Đặt một giá trị trên một nhóm, hoặc trên một property có kiểu chưa được đăng ký, trả về `false` và không phát gì.

**Double có nhiều chữ số hơn số được hiển thị (từ 1.9).** Khi `Attr::RoundToDecimals` được đặt là `false`, một `Double`
giữ mọi chữ số mà nó nhận được và chỉ bị clamp vào khoảng của nó; `Attr::Decimals` khi đó đặt số chữ số hiển thị trong
một ô và trong spin box. Việc hiển thị giá trị, làm mới editor, commit một editor mà người dùng không đổi và
`resetToDefault()` đều để nguyên giá trị đã lưu. Một giá trị mà người dùng đổi trong editor (một bước, văn bản gõ vào)
được lưu đúng như spin box đang giữ nó, đã làm tròn theo số chữ số thập phân của nó; gõ đúng văn bản đang được hiển thị
được tính là không thay đổi (với 0.0125 đã lưu và 0.01 đang hiển thị, gõ 0.01 giữ nguyên 0.0125). Tuần tự hóa giữ con
số đã lưu. Mặc định `true` làm tròn như trước.

```cpp
qpb::Property* scale = model.find("scale");   // a Double with decimals 2
scale->setAttribute(qpb::Attr::RoundToDecimals, false);
scale->setValue(0.0125);                       // stores 0.0125, shows 0.01
```

**Đổi một attribute không bao giờ tự thay đổi giá trị.** Sau khi `setAttribute()` hạ một giá trị tối đa hoặc một
số chữ số thập phân, giá trị đã lưu vẫn như cũ, kể cả khi bây giờ nó nằm ngoài khoảng. Để đưa các giá trị vào trong các
attribute hiện tại, hãy cho chúng chạy qua pipeline một lần nữa (từ 1.9):

- `Property::normalizeValue()` làm với giá trị hiện tại điều mà `setValue(value())` làm: chuyển đổi, chuẩn hóa, xác thực
  kiểu và validator, như code ứng dụng (kể cả các property read-only và disabled). Nó phát `valueChanged` (và không phát
  `userValueChanged`) chỉ khi giá trị thực sự thay đổi, và trả về `false`, giữ nguyên giá trị và phát `validationFailed`,
  nếu một kiểm tra bây giờ từ chối nó; nó cũng trả về `false`, một cách im lặng, với một kiểu chưa được đăng ký. Trên một
  nhóm, nó chuẩn hóa mọi phần tử con cháu trừ [các property live](changes-and-conditions.md#giá-trị-live) trong một
  batch và trả về `false` nếu có phần tử nào bị từ chối.
- `PropertyModel::normalizeAllValues()` làm điều tương tự cho cả cây, giống `root()->normalizeValue()`; nó trả về
  `true` với một model không có gốc.

```cpp
property->setAttribute(qpb::Attr::Maximum, 100);
property->normalizeValue();     // 150 becomes 100
model.normalizeAllValues();     // after rebuilding all the ranges
```

## Ai được đổi cái gì

`readOnly` và `disabled` chỉ chặn người dùng: các chỉnh sửa thực hiện qua một view bị từ chối. Code ứng dụng vẫn có thể
đổi các property như vậy bằng `PropertyModel::setValue()`, `Property::setValue()` và `resetToDefault()`, ví dụ một
trường trạng thái read-only mà chương trình của bạn duy trì. Cả hai hàm đều trả về `bool`: `true` khi giá trị được chấp
nhận (hoặc đã là giá trị hiện tại), `false` khi nó bị từ chối.

## Các kiểm tra có sẵn

| Kiểu | Kiểm tra |
|---|---|
| Int, Int64 | Không cần: giá trị được clamp vào khoảng, nên luôn hợp lệ. |
| Double | Giá trị không phải NaN (thông điệp là "The value is not a number"); mọi số khác được clamp vào khoảng. |
| String | Tối đa `Attr::MaxLength` ký tự; toàn bộ giá trị khớp `Attr::RegularExpression`. |
| Enum | Giá trị là một trong các option (`Attr::Options`). |
| FilePath | Với `Attr::MustExist` và `FileMode::Open`, một giá trị không trống phải là tên của một tệp đang tồn tại. Chuỗi rỗng luôn hợp lệ. |
| DirPath | Với `Attr::MustExist`, một giá trị không trống phải là tên của một thư mục đang tồn tại. Chuỗi rỗng luôn hợp lệ. |
| Bool | Chỉ chuyển đổi. |

Các thông điệp của những kiểm tra này, và của một giá trị không chuyển đổi được, đều được dịch (xem
[Bản dịch](translations.md)). Câu chữ của chúng không thuộc API: hãy so sánh `ValidationResult::ok`, đừng bao giờ so
sánh văn bản của một thông điệp.

## Validator của riêng bạn

Một validator chạy sau các kiểm tra của kiểu. Nó nhận giá trị ứng viên và property, nên nó có thể đọc attribute hoặc xem
xét các property anh em. Hãy trả về `ValidationResult::valid()` hoặc `ValidationResult::error(message)`:

```cpp
root->addString("name", "Main").validator([&scene](const QVariant& v, const qpb::Property&) {
    return scene.contains(v.toString()) ? qpb::ValidationResult::error("Name already used")
                                        : qpb::ValidationResult::valid();
});
```

Hãy đưa thông điệp đã được dịch sẵn (ví dụ bằng `tr()`). Một validator cũng có thể được đặt sau đó bằng
`Property::setValidator()`. Để kiểm tra các giá trị của cả một kiểu, hãy đăng ký một kiểu với hàm `validate`; xem
[Kiểu và editor tùy biến](custom-types-and-editors.md).

## Giá trị mặc định và đặt lại

Giá trị mặc định là giá trị mà property có khi nó được tạo; đổi nó bằng `setDefaultValue()`. `isModified()` là true khi
giá trị khác giá trị mặc định; các view hiển thị tên của các property đã sửa bằng chữ đậm. Nhóm không bao giờ là đã sửa,
và [các property live](changes-and-conditions.md#giá-trị-live) cũng vậy.

- `Property::resetToDefault()` đặt giá trị trở lại qua pipeline ở trên và trả về `false` nếu việc đặt lại bị từ chối.
  Trên một nhóm, nó đặt lại mọi phần tử con cháu trừ các property live.
- `PropertyModel::resetToDefault(index)` làm điều tương tự cho property hoặc nhóm tại một index và phát một
  `batchValueChanged`. Một index không hợp lệ, kể cả index của gốc, trả về `false`.
- `PropertyModel::resetAllToDefault()` (từ 1.5) đặt lại cả cây giống `root()->resetToDefault()`: cả các property
  read-only và disabled, không đặt lại property live, một `batchValueChanged`. Nó trả về `false` nếu một lần đặt lại bị
  từ chối, và `true` với một model không có gốc.
- Các view cung cấp "Reset to default" cho một property và "Reset group" cho một nhóm trong menu ngữ cảnh của chúng.
- `Property::makeValueDefault()` (từ 1.9) biến giá trị hiện tại thành giá trị mặc định: `defaultValue()` trở thành
  `value()` và property không còn là đã sửa, ví dụ sau khi nạp cài đặt, để các chỉnh sửa về sau được hiển thị là đã sửa
  so với lần nạp đó. Nó không đổi giá trị nào, nên không phát `valueChanged`; các view chỉ vẽ lại những hàng có giá trị
  mặc định đã đổi. Trên một nhóm, nó áp dụng cho mọi phần tử con cháu trừ các property live.
- `PropertyModel::makeAllValuesDefault()` (từ 1.9) làm điều này cho cả cây, giống `root()->makeValueDefault()`.
