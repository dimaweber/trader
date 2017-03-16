#ifndef TABLEFIELD_H
#define TABLEFIELD_H

#include <QString>
#include <QStringList>

class TableField
{
public:
    enum Types {Integer, Decimal, Char, Boolean, BigInt, Double, Varchar, Datetime};

    TableField(const QString& name, Types type = Integer, int size=0, qint16 prec=-1)
        :_name(name), _type(type), _size(size), _prec(prec),
          _notNull(false), _primaryKey(false), _autoIncrement(false)
    {}

    TableField& notNull()
    {
        _notNull = true;
        return *this;
    }

    TableField& defaultValue(const QString& value)
    {
        _default = value;
        return *this;
    }

    TableField& defaultValue(float value)
    {
        _default = QString::number(value, 'f');
        return *this;
    }

    TableField& references(const QString& tableName, const QStringList& fields = QStringList())
    {
        _refTable = tableName;
        _refFields = fields;
        return *this;
    }

    TableField& primaryKey(bool isAutoInc = true)
    {
        _primaryKey = true;
        _autoIncrement = isAutoInc;
        return *this;
    }

    TableField& check(const QString& check)
    {
        _check = check;
        return *this;
    }

    operator QString() const
    {
        QString t;
        switch(_type)
        {
            case Decimal: t = "DECIMAL"; break;
            case Integer:  t = "INTEGER"; break;
            case Char: t = "CHAR"; break;
            case Boolean: t = "BOOLEAN"; break;
            case BigInt: t = "BIGINT"; break;
            case Double: t = "DOUBLE"; break;
            case Varchar: t = "VARCHAR"; break;
            case Datetime: t = "DATETIME"; break;
        }

        QString ret = QString("%1 %2").arg(_name).arg(t);
        if (_size > 0)
        {
            if (_prec == -1)
                ret += QString(" (%1)").arg(_size);
            else
                ret += QString(" (%1, %2)").arg(_size).arg(_prec);
        }
        if (_primaryKey)
            ret +=  " PRIMARY KEY ";
        if (_autoIncrement)
            ret += " AUTO_INCREMENT ";
        if (_notNull)
            ret += " NOT NULL";
        if (!_default.isEmpty())
        {
            if (t == "VARCHAR" || t == "CHAR")
                ret += QString(" DEFAULT '%1'").arg(_default);
            else
                ret += QString(" DEFAULT %1").arg(_default);
        }
        if (!_check.isEmpty())
        {
            ret += " CHECK (" + _check + ")";
        }
        if (!_refTable.isEmpty())
        {
            ret += QString (" REFERENCES %1").arg(_refTable);
            if (!_refFields.isEmpty())
                ret += QString(" (%1)").arg(_refFields.join(','));
        }

        return ret;
    }
private:
    QString _name;
    Types _type;
    int _size;
    int _prec;
    bool _notNull;
    bool _primaryKey;
    bool _autoIncrement;
    QString _default;
    QString _refTable;
    QString _check;
    QStringList _refFields;
};

#endif // TABLEFIELD_H
