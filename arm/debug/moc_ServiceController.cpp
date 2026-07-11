/****************************************************************************
** Meta object code from reading C++ file 'ServiceController.hpp'
**
** Created by: The Qt Meta Object Compiler version 63 (Qt 4.8.6)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/ServiceController.hpp"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ServiceController.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.6. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_ServiceController[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      27,   19,   18,   18, 0x08,
      64,   18,   18,   18, 0x08,
      78,   18,   18,   18, 0x08,
      97,   18,   18,   18, 0x08,
     111,   18,   18,   18, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_ServiceController[] = {
    "ServiceController\0\0request\0"
    "onInvoked(bb::system::InvokeRequest)\0"
    "onPollTimer()\0onSessionExpired()\0"
    "onHeartbeat()\0onAboutToQuit()\0"
};

void ServiceController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        ServiceController *_t = static_cast<ServiceController *>(_o);
        switch (_id) {
        case 0: _t->onInvoked((*reinterpret_cast< const bb::system::InvokeRequest(*)>(_a[1]))); break;
        case 1: _t->onPollTimer(); break;
        case 2: _t->onSessionExpired(); break;
        case 3: _t->onHeartbeat(); break;
        case 4: _t->onAboutToQuit(); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData ServiceController::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject ServiceController::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_ServiceController,
      qt_meta_data_ServiceController, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &ServiceController::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *ServiceController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *ServiceController::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_ServiceController))
        return static_cast<void*>(const_cast< ServiceController*>(this));
    return QObject::qt_metacast(_clname);
}

int ServiceController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
