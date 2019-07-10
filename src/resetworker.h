#pragma once

#include <nan.h>

#include "bossajs.h"

using namespace v8;

class ResetWorker : public Nan::AsyncWorker {
private:
  Bossa *bossa;
  std::string port;

public:
  ResetWorker(Nan::Callback *callback, Bossa *bossa, std::string port) : Nan::AsyncWorker(callback), bossa(bossa), port(port) {}
  ~ResetWorker() {}

  void Execute();
  void HandleOKCallback();
  void HandleErrorCallback();
};
