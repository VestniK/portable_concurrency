#include <type_traits>

#include <QtCore/QtDebug>

#include <QtCore/QCoreApplication>
#include <QtCore/QRunnable>
#include <QtCore/QThreadPool>

#include <portable_concurrency/functional>
#include <portable_concurrency/future>

#include "PCFutureWatcher.h"

namespace portable_concurrency {
template<>
struct is_executor<QThreadPool*>: std::true_type {};
};

void post(QThreadPool* exec, pc::unique_function<void()> func) {
  struct PCRunnable: QRunnable {
    PCRunnable(pc::unique_function<void()>&& func) noexcept: func{std::move(func)} {}

    void run() override {func();}

    pc::unique_function<void()> func;
  };
  exec->start(new PCRunnable{std::move(func)});
}

int main(int argc, char** argv) {
  QCoreApplication app{argc, argv};

  pc::future<QString> f = pc::async(QThreadPool::globalInstance(), []() {
    QThread::msleep(800);
    return QStringLiteral("Hello Qt Concurent");
  });
  PCFutureWatcher watcher;
  QObject::connect(&watcher, &PCFutureWatcher::finished, [&f]() {qInfo() << f.get();});
  QObject::connect(&watcher, &PCFutureWatcher::finished, &app, &QCoreApplication::quit);
  watcher.setFuture(f);

  return  app.exec();
}
