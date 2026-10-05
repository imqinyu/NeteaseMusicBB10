/****************************************************************************
** Meta object code from reading C++ file 'NmApi.hpp'
**
** Created: Fri Oct 2 19:40:18 2026
**      by: The Qt Meta Object Compiler version 63 (Qt 4.8.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/nm/api/NmApi.hpp"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'NmApi.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_nm__NmApi[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: signature, parameters, type, tag, flags
      28,   11,   10,   10, 0x05,
      75,   11,   10,   10, 0x05,
     150,  126,   10,   10, 0x05,
     206,   11,   10,   10, 0x05,
     284,  255,   10,   10, 0x05,

 // slots: signature, parameters, type, tag, flags
     359,  331,   10,   10, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_nm__NmApi[] = {
    "nm::NmApi\0\0requestId,result\0"
    "searchFinished(int,nm::NmParsers::SearchParse)\0"
    "playlistFinished(int,nm::NmParsers::PlaylistParse)\0"
    "requestId,songId,result\0"
    "songUrlFinished(int,qint64,nm::NmParsers::SongUrlParse)\0"
    "accountFinished(int,nm::NmParsers::AccountParse)\0"
    "requestId,tag,message,detail\0"
    "requestFailed(int,QString,QString,QVariantMap)\0"
    "requestId,tag,json,response\0"
    "onHttpFinished(int,QString,nm::NmJson,nm::NmHttpResponse)\0"
};

void nm::NmApi::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        NmApi *_t = static_cast<NmApi *>(_o);
        switch (_id) {
        case 0: _t->searchFinished((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const nm::NmParsers::SearchParse(*)>(_a[2]))); break;
        case 1: _t->playlistFinished((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const nm::NmParsers::PlaylistParse(*)>(_a[2]))); break;
        case 2: _t->songUrlFinished((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< qint64(*)>(_a[2])),(*reinterpret_cast< const nm::NmParsers::SongUrlParse(*)>(_a[3]))); break;
        case 3: _t->accountFinished((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const nm::NmParsers::AccountParse(*)>(_a[2]))); break;
        case 4: _t->requestFailed((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< const QVariantMap(*)>(_a[4]))); break;
        case 5: _t->onHttpFinished((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const nm::NmJson(*)>(_a[3])),(*reinterpret_cast< const nm::NmHttpResponse(*)>(_a[4]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData nm::NmApi::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject nm::NmApi::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_nm__NmApi,
      qt_meta_data_nm__NmApi, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &nm::NmApi::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *nm::NmApi::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *nm::NmApi::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_nm__NmApi))
        return static_cast<void*>(const_cast< NmApi*>(this));
    return QObject::qt_metacast(_clname);
}

int nm::NmApi::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void nm::NmApi::searchFinished(int _t1, const nm::NmParsers::SearchParse & _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void nm::NmApi::playlistFinished(int _t1, const nm::NmParsers::PlaylistParse & _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void nm::NmApi::songUrlFinished(int _t1, qint64 _t2, const nm::NmParsers::SongUrlParse & _t3)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)), const_cast<void*>(reinterpret_cast<const void*>(&_t3)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void nm::NmApi::accountFinished(int _t1, const nm::NmParsers::AccountParse & _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void nm::NmApi::requestFailed(int _t1, const QString & _t2, const QString & _t3, const QVariantMap & _t4)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)), const_cast<void*>(reinterpret_cast<const void*>(&_t3)), const_cast<void*>(reinterpret_cast<const void*>(&_t4)) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}
QT_END_MOC_NAMESPACE
