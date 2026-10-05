/****************************************************************************
** Meta object code from reading C++ file 'NmHttpClient.hpp'
**
** Created: Fri Oct 2 19:40:18 2026
**      by: The Qt Meta Object Compiler version 63 (Qt 4.8.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/nm/net/NmHttpClient.hpp"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'NmHttpClient.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_nm__NmHttpClient[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: signature, parameters, type, tag, flags
      46,   18,   17,   17, 0x05,

 // slots: signature, parameters, type, tag, flags
      98,   17,   17,   17, 0x08,
     116,   17,   17,   17, 0x08,
     148,  133,   17,   17, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_nm__NmHttpClient[] = {
    "nm::NmHttpClient\0\0requestId,tag,json,response\0"
    "finished(int,QString,nm::NmJson,nm::NmHttpResponse)\0"
    "onReplyFinished()\0onReplyTimeout()\0"
    "received,total\0onReplyProgress(qint64,qint64)\0"
};

void nm::NmHttpClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        NmHttpClient *_t = static_cast<NmHttpClient *>(_o);
        switch (_id) {
        case 0: _t->finished((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const nm::NmJson(*)>(_a[3])),(*reinterpret_cast< const nm::NmHttpResponse(*)>(_a[4]))); break;
        case 1: _t->onReplyFinished(); break;
        case 2: _t->onReplyTimeout(); break;
        case 3: _t->onReplyProgress((*reinterpret_cast< qint64(*)>(_a[1])),(*reinterpret_cast< qint64(*)>(_a[2]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData nm::NmHttpClient::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject nm::NmHttpClient::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_nm__NmHttpClient,
      qt_meta_data_nm__NmHttpClient, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &nm::NmHttpClient::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *nm::NmHttpClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *nm::NmHttpClient::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_nm__NmHttpClient))
        return static_cast<void*>(const_cast< NmHttpClient*>(this));
    return QObject::qt_metacast(_clname);
}

int nm::NmHttpClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void nm::NmHttpClient::finished(int _t1, const QString & _t2, const nm::NmJson & _t3, const nm::NmHttpResponse & _t4)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)), const_cast<void*>(reinterpret_cast<const void*>(&_t3)), const_cast<void*>(reinterpret_cast<const void*>(&_t4)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_END_MOC_NAMESPACE
