#include <qpb/Attributes.h>
#include <qpb/Property.h>
#include <qpb/TypeRegistry.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qhash.h>
#include <QtCore/qlocale.h>
#include <QtCore/qregularexpression.h>

#include <cmath>
#include <limits>
#include <vector>

namespace qpb {

namespace detail {

class TypeRegistryPrivate
{
public:
    void insert(const TypeId& id, const TypeHandler& handler)
    {
        index.insert(id, handlers.size());
        handlers.push_back(std::make_unique<TypeHandler>(handler));
        order.append(id);
    }

    // Stable addresses: handler() returns pointers into this storage.
    std::vector<std::unique_ptr<TypeHandler>> handlers;
    QHash<TypeId, size_t> index;
    QList<TypeId> order;
};

} // namespace detail

namespace {

QString affixed(const QString& number, const Property& property)
{
    return property.attribute(Attr::Prefix).toString() + number
        + property.attribute(Attr::Suffix).toString();
}

// --- Bool ------------------------------------------------------------------------

TypeHandler boolHandler()
{
    TypeHandler handler;
    handler.storageType = QMetaType::fromType<bool>();
    return handler;
}

// --- Int -------------------------------------------------------------------------

TypeHandler intHandler()
{
    TypeHandler handler;
    handler.storageType = QMetaType::fromType<int>();
    handler.displayText = [](const QVariant& value, const Property& property) {
        return affixed(QLocale().toString(value.toInt()), property);
    };
    handler.normalize = [](const QVariant& value, const Property& property) {
        const int minimum
            = property.attribute(Attr::Minimum, std::numeric_limits<int>::min()).toInt();
        const int maximum
            = property.attribute(Attr::Maximum, std::numeric_limits<int>::max()).toInt();
        return QVariant(qBound(minimum, value.toInt(), qMax(minimum, maximum)));
    };
    return handler;
}

// --- Double ----------------------------------------------------------------------

constexpr int DefaultDecimals = 2;

int decimalsOf(const Property& property)
{
    return qBound(0, property.attribute(Attr::Decimals, DefaultDecimals).toInt(), 15);
}

TypeHandler doubleHandler()
{
    TypeHandler handler;
    handler.storageType = QMetaType::fromType<double>();
    handler.displayText = [](const QVariant& value, const Property& property) {
        const QLocale locale;
        QString text = locale.toString(value.toDouble(), 'f', decimalsOf(property));
        const QString point = locale.decimalPoint();
        if (text.contains(point)) {
            while (text.endsWith(locale.zeroDigit()))
                text.chop(locale.zeroDigit().size());
            if (text.endsWith(point))
                text.chop(point.size());
        }
        return affixed(text, property);
    };
    handler.normalize = [](const QVariant& value, const Property& property) {
        double number = value.toDouble();
        if (std::isnan(number))
            return value;
        const double minimum
            = property.attribute(Attr::Minimum, -std::numeric_limits<double>::infinity())
                  .toDouble();
        const double maximum
            = property.attribute(Attr::Maximum, std::numeric_limits<double>::infinity()).toDouble();
        number = qBound(minimum, number, qMax(minimum, maximum));
        if (std::isfinite(number)) {
            const double scale = std::pow(10.0, decimalsOf(property));
            const double rounded = std::round(number * scale) / scale;
            if (std::isfinite(rounded))
                number = rounded;
        }
        return QVariant(number);
    };
    handler.validate = [](const QVariant& value, const Property&) {
        return std::isnan(value.toDouble())
            ? ValidationResult::error(QStringLiteral("The value is not a number"))
            : ValidationResult::valid();
    };
    return handler;
}

// --- String ----------------------------------------------------------------------

TypeHandler stringHandler()
{
    TypeHandler handler;
    handler.storageType = QMetaType::fromType<QString>();
    handler.validate = [](const QVariant& value, const Property& property) {
        const QString text = value.toString();
        if (property.hasAttribute(Attr::MaxLength)) {
            const int maxLength = property.attribute(Attr::MaxLength).toInt();
            if (maxLength >= 0 && text.size() > maxLength)
                return ValidationResult::error(
                    QStringLiteral("At most %1 characters are allowed").arg(maxLength));
        }
        const QString pattern = property.attribute(Attr::RegularExpression).toString();
        if (!pattern.isEmpty()) {
            const QRegularExpression expression(QRegularExpression::anchoredPattern(pattern));
            if (!expression.isValid()) {
                qWarning("qpb: invalid regular expression \"%s\" on property \"%s\": %s",
                    qUtf8Printable(pattern), qUtf8Printable(property.path()),
                    qUtf8Printable(expression.errorString()));
            } else if (!expression.match(text).hasMatch()) {
                return ValidationResult::error(
                    QStringLiteral("The value does not match the pattern %1").arg(pattern));
            }
        }
        return ValidationResult::valid();
    };
    return handler;
}

// --- Enum ------------------------------------------------------------------------

QList<EnumOption> optionsOf(const Property& property)
{
    return property.attribute(Attr::Options).value<QList<EnumOption>>();
}

TypeHandler enumHandler()
{
    TypeHandler handler; // values are int or QString: stored unconverted
    handler.displayText = [](const QVariant& value, const Property& property) {
        for (const EnumOption& option : optionsOf(property)) {
            if (option.value == value)
                return option.label;
        }
        return value.toString();
    };
    // Map a value of another type ("1" for 1, 1 for "1") onto the matching option.
    handler.normalize = [](const QVariant& value, const Property& property) {
        const QList<EnumOption> options = optionsOf(property);
        for (const EnumOption& option : options) {
            if (option.value == value)
                return option.value;
        }
        for (const EnumOption& option : options) {
            QVariant converted = value;
            if (converted.convert(option.value.metaType()) && converted == option.value)
                return option.value;
        }
        return value;
    };
    handler.validate = [](const QVariant& value, const Property& property) {
        for (const EnumOption& option : optionsOf(property)) {
            if (option.value == value)
                return ValidationResult::valid();
        }
        return ValidationResult::error(
            QStringLiteral("\"%1\" is not one of the options").arg(value.toString()));
    };
    return handler;
}

// --- FilePath / DirPath ----------------------------------------------------------

QString nativePath(const QVariant& value, const Property&)
{
    return QDir::toNativeSeparators(value.toString());
}

TypeHandler filePathHandler()
{
    TypeHandler handler;
    handler.storageType = QMetaType::fromType<QString>();
    handler.displayText = nativePath;
    handler.validate = [](const QVariant& value, const Property& property) {
        const QString path = value.toString();
        const bool mustExist = property.attribute(Attr::MustExist, false).toBool();
        const auto mode
            = FileMode(property.attribute(Attr::DialogMode, int(FileMode::Open)).toInt());
        if (path.isEmpty() || !mustExist || mode != FileMode::Open || QFileInfo(path).isFile())
            return ValidationResult::valid();
        return ValidationResult::error(QStringLiteral("The file %1 does not exist").arg(path));
    };
    return handler;
}

TypeHandler dirPathHandler()
{
    TypeHandler handler;
    handler.storageType = QMetaType::fromType<QString>();
    handler.displayText = nativePath;
    handler.validate = [](const QVariant& value, const Property& property) {
        const QString path = value.toString();
        const bool mustExist = property.attribute(Attr::MustExist, false).toBool();
        if (path.isEmpty() || !mustExist || QFileInfo(path).isDir())
            return ValidationResult::valid();
        return ValidationResult::error(QStringLiteral("The directory %1 does not exist").arg(path));
    };
    return handler;
}

bool isReserved(const TypeId& id)
{
    return id.isEmpty() || id.startsWith(QLatin1String("qpb.")) || id == Types::Group;
}

} // namespace

TypeRegistry& TypeRegistry::global()
{
    static TypeRegistry registry;
    return registry;
}

TypeRegistry::TypeRegistry()
    : d(std::make_unique<detail::TypeRegistryPrivate>())
{
    d->insert(Types::Bool, boolHandler());
    d->insert(Types::Int, intHandler());
    d->insert(Types::Double, doubleHandler());
    d->insert(Types::String, stringHandler());
    d->insert(Types::Enum, enumHandler());
    d->insert(Types::FilePath, filePathHandler());
    d->insert(Types::DirPath, dirPathHandler());
}

TypeRegistry::~TypeRegistry() = default;

bool TypeRegistry::registerType(const TypeId& id, const TypeHandler& handler)
{
    if (isReserved(id) || contains(id))
        return false;
    d->insert(id, handler);
    return true;
}

bool TypeRegistry::replaceType(const TypeId& id, const TypeHandler& handler)
{
    const auto it = d->index.constFind(id);
    if (it == d->index.constEnd())
        return false;
    *d->handlers[*it] = handler;
    return true;
}

bool TypeRegistry::contains(const TypeId& id) const
{
    return d->index.contains(id);
}

const TypeHandler* TypeRegistry::handler(const TypeId& id) const
{
    const auto it = d->index.constFind(id);
    return it == d->index.constEnd() ? nullptr : d->handlers[*it].get();
}

QList<TypeId> TypeRegistry::types() const
{
    return d->order;
}

} // namespace qpb
