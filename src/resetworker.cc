#include "resetworker.h"
#include "util.h"

using namespace v8;

void ResetWorker::Execute() {
  try {
    bossa->arduinoReset(port);
  } catch (const std::exception &exc) {
    SetErrorMessage(exc.what());
  }
}

void ResetWorker::HandleOKCallback() {
  Nan::HandleScope scope;

  Local<Value> argv[] = {
      Nan::Null(), // err
  };

  callback->Call(1, argv, async_resource);
}

void ResetWorker::HandleErrorCallback() {
  Nan::HandleScope scope;

  Local<Value> argv[] = {
      Nan::Error(this->ErrorMessage()), // err
  };

  callback->Call(1, argv, async_resource);
}