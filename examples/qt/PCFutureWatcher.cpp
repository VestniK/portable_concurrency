#include <QtCore/QMetaObject>
#include <QtCore/QMutex>

#include "PCFutureWatcher.h"

struct PCFutureWatcher::SafeRef {
  SafeRef(PCFutureWatcher* obj): ref(obj) {}

  PCFutureWatcher* ref;
  QMutex mutex;
};

PCFutureWatcher::PCFutureWatcher(QObject* parent): QObject(parent)
{}

PCFutureWatcher::~PCFutureWatcher() {
  detachCurrRef();
}

void PCFutureWatcher::notify(SafeRef& ref) {
  QMutexLocker lock{&ref.mutex};
  if (!ref.ref)
    return;
  QMetaObject::invokeMethod(ref.ref, "finished", Qt::QueuedConnection);
}

std::shared_ptr<PCFutureWatcher::SafeRef> PCFutureWatcher::createNewRef() {
  detachCurrRef();
  return cur_ref_ = std::make_shared<SafeRef>(this);
}

void PCFutureWatcher::detachCurrRef() {
  if (!cur_ref_)
    return;
  QMutexLocker{&cur_ref_->mutex}, cur_ref_->ref = nullptr;
  cur_ref_.reset();
}
