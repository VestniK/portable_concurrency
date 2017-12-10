#pragma once

#include <memory>

#include <QtCore/QObject>

#include <portable_concurrency/future>

class PCFutureWatcher: public QObject {
  Q_OBJECT
public:
  PCFutureWatcher(QObject* parent = nullptr);
  ~PCFutureWatcher();

  template<typename T>
  void setFuture(pc::future<T>& future) {
    future = future.then([ref = createNewRef()](pc::future<T> ready) {
      PCFutureWatcher::notify(*ref);
      return ready;
    });
  }

  template<typename T>
  void setFuture(pc::shared_future<T>& future) {
    future = future.then([ref = createNewRef()](pc::future<T> ready) {
      PCFutureWatcher::notify(*ref);
      return ready;
    });
  }

signals:
  void finished();

private:
  struct SafeRef;

  static void notify(SafeRef& ref);
  std::shared_ptr<SafeRef> createNewRef();
  void detachCurrRef();

private:
  std::shared_ptr<SafeRef> cur_ref_;
};
