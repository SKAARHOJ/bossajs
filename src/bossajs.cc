#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <thread>

#include "bossajs.h"
#include "closeworker.h"
#include "connectworker.h"
#include "eraseworker.h"
#include "infoworker.h"
#include "readworker.h"
#include "resetworker.h"
#include "util.h"
#include "verifyworker.h"
#include "writeworker.h"

using namespace v8;

Nan::Persistent<FunctionTemplate> Bossa::constructor;

void Bossa::connect(std::string portStr) {
  if (portStr.empty()) {
    portStr = portFactory.def();
  }

  if (portStr.empty()) {
    throw std::runtime_error("No serial ports available");
  }
  SerialPort::Ptr port;

  port = portFactory.create(portStr);

  /*
  // Reset has to be done manually, no need to do it on connect!
    printf("Arduino 1200 baud reset\n");
    if (!port->open(1200)) {
      throw std::runtime_error("Failed to open port at 1200bps\n");
    }
     */

  port->setRTS(true);
  port->setDTR(false);
  port->close();

  // wait for chip to reboot and USB port to re-appear
  std::this_thread::sleep_for(std::chrono::milliseconds(3));

  if (!samba.connect(portFactory.create(portStr))) {
    throw std::runtime_error("No device found on " + portStr);
  }

  device.emplace(samba);
  device->create();

  flasher.emplace(samba, *device, observer);

  connected = true;
}

void Bossa::reset(std::string portStr) {
  // Arduinoreset
  if (portStr.empty()) {
    portStr = portFactory.def();
  }

  if (portStr.empty()) {
    throw std::runtime_error("No serial ports available");
  }
  SerialPort::Ptr port;

  port = portFactory.create(portStr);

  printf("Arduino 1200 baud reset\n");
  if (!port->open(1200)) {
    throw std::runtime_error("Failed to open port at 1200bps\n");
  }

  port->setRTS(true);
  port->setDTR(false);
  port->close();

  // wait for chip to reboot and USB port to re-appear
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

void Bossa::close() {

  // device.reset();
  Device::FlashPtr &flash = device->getFlash();
  flash->setBootFlash(true);
  flash->writeOptions();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // wait for flash write

  device->reset();

  samba.disconnect();
}

void Bossa::erase(uint32_t offset) { flasher->erase(offset); }

void Bossa::info(FlasherInfo &finfo) {
  if (!connected) {
    throw NotConnected();
  }

  flasher->info(finfo);
}

std::vector<char> // FIXME: potentially very big
Bossa::read(uint32_t offset, uint32_t size) {
  // FIXME: deprecated
  const char *filename = std::tmpnam(nullptr);

  try {
    flasher->read(filename, size, offset);

  } catch (...) {
    std::remove(filename);

    throw;
  }

  // Reopen the file to copy its contents out
  std::ifstream file(filename, std::ios::binary);

  try {
    std::vector<char> content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    file.close();
    std::remove(filename);

    return content;
  } catch (...) {
    file.close();
    std::remove(filename);

    throw;
  }

  throw std::logic_error("not reached");
}

bool Bossa::verify(Nan::TypedArrayContents<uint8_t> &buffer, uint32_t offset) {
  // FIXME: deprecated
  const char *filename = std::tmpnam(nullptr);

  // Write the buffer to disk
  std::ofstream file(filename, std::ios::binary);

  try {
    file.write(reinterpret_cast<char *>(*buffer), buffer.length());
    file.close();
  } catch (...) {
    file.close();
    std::remove(filename);

    throw;
  }

  try {
    uint32_t pageErrors;
    uint32_t totalErrors;

    bool result = flasher->verify(filename, pageErrors, totalErrors, offset);
    std::remove(filename);

    return result;
  } catch (...) {
    std::remove(filename);

    throw;
  }

  throw std::logic_error("not reached");
}

void Bossa::write(Nan::TypedArrayContents<uint8_t> &buffer, uint32_t offset) {
  // FIXME: deprecated
  const char *filename = std::tmpnam(nullptr);

  // Write the buffer to disk
  std::ofstream file(filename, std::ios::binary);

  try {
    file.write(reinterpret_cast<char *>(*buffer), buffer.length());
    file.close();
  } catch (...) {
    file.close();
    std::remove(filename);

    throw;
  }

  try {
    flasher->write(filename, offset);
    std::remove(filename);
  } catch (...) {
    std::remove(filename);

    throw;
  }
}

NAN_MODULE_INIT(Bossa::Init) {
  Local<FunctionTemplate> ctor = Nan::New<FunctionTemplate>(New);

  constructor.Reset(ctor);

  ctor->InstanceTemplate()->SetInternalFieldCount(1);
  ctor->SetClassName(L("Bossa"));

  // Link getters and setters

  // Set Methods into Prototype
  Nan::SetPrototypeMethod(ctor, "connect", Connect);
  Nan::SetPrototypeMethod(ctor, "close", Close);
  Nan::SetPrototypeMethod(ctor, "erase", Erase);
  Nan::SetPrototypeMethod(ctor, "reset", Reset);
  Nan::SetPrototypeMethod(ctor, "info", Info);
  Nan::SetPrototypeMethod(ctor, "read", Read);
  Nan::SetPrototypeMethod(ctor, "verify", Verify);
  Nan::SetPrototypeMethod(ctor, "write", Write);

  // Export class
  
  Local<Context> context;
  target->Set(L("Bossa"), ctor->GetFunction(context).ToLocalChecked());
  target->Set(L("default"), ctor->GetFunction(context).ToLocalChecked());
}

NAN_METHOD(Bossa::New) {
  bool debug = false;

  if (!info.IsConstructCall()) {
    return Nan::ThrowError("Must call with new keyword");
  }

  if (info.Length() == 0) {
    // Pass
  } else if (info.Length() == 1) {
    if (!info[0]->IsObject()) {
      return Nan::ThrowTypeError("opts must be an object");
    } else {
      // Load config
      Local config = Nan::To<Object>(info[0]).ToLocalChecked();
      debug = Nan::To<bool>(OBJ_GET(config, "debug")).FromMaybe(false);
    }
  } else {
    return Nan::ThrowTypeError("opts must be an object or nothing");
  }

  try {
    Bossa *self = new Bossa(debug);

    self->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } catch (const std::exception &exc) {
    return Nan::ThrowError(exc.what());
  }
}

NAN_METHOD(Bossa::Connect) {
  Bossa *self = Nan::ObjectWrap::Unwrap<Bossa>(info.This());
  std::string port;

  if (info.Length() != 2) {
    return Nan::ThrowTypeError("Must provide port and callback");
  }

  if (!info[0]->IsString()) {
    return Nan::ThrowTypeError("port must be a string");
  }
  Local value = Nan::To<String>(info[0]).ToLocalChecked();

  if (!value->IsNullOrUndefined()) {
    Nan::Utf8String port_(value);
    port = std::string(*port_);
  }

  if (!info[1]->IsFunction()) {
    return Nan::ThrowTypeError("callback must be a function");
  }
  Nan::Callback *callback = new Nan::Callback(Nan::To<Function>(info[1]).ToLocalChecked());

  Nan::AsyncQueueWorker(new ConnectWorker(callback, self, port));
}

NAN_METHOD(Bossa::Reset) {
  Bossa *self = Nan::ObjectWrap::Unwrap<Bossa>(info.This());
  std::string port;

  if (info.Length() != 2) {
    return Nan::ThrowTypeError("Must provide port and callback");
  }

  if (!info[0]->IsString()) {
    return Nan::ThrowTypeError("port must be a string");
  }
  Local value = Nan::To<String>(info[0]).ToLocalChecked();

  if (!value->IsNullOrUndefined()) {
    Nan::Utf8String port_(value);
    port = std::string(*port_);
  }

  if (!info[1]->IsFunction()) {
    return Nan::ThrowTypeError("callback must be a function");
  }
  Nan::Callback *callback = new Nan::Callback(Nan::To<Function>(info[1]).ToLocalChecked());

  Nan::AsyncQueueWorker(new ResetWorker(callback, self, port));
}

NAN_METHOD(Bossa::Close) {
  Bossa *self = Nan::ObjectWrap::Unwrap<Bossa>(info.This());

  if (info.Length() != 1) {
    return Nan::ThrowTypeError("Must provide callback");
  }

  if (!info[0]->IsFunction()) {
    return Nan::ThrowTypeError("callback must be a function");
  }
  Nan::Callback *callback = new Nan::Callback(Nan::To<Function>(info[0]).ToLocalChecked());

  Nan::AsyncQueueWorker(new CloseWorker(callback, self));
}

NAN_METHOD(Bossa::Info) {
  Bossa *self = Nan::ObjectWrap::Unwrap<Bossa>(info.This());

  if (info.Length() != 1 || !info[0]->IsFunction()) {
    return Nan::ThrowTypeError("callback must be a function");
  }

  Nan::Callback *callback = new Nan::Callback(Nan::To<Function>(info[0]).ToLocalChecked());

  Nan::AsyncQueueWorker(new InfoWorker(callback, self));
}

NAN_METHOD(Bossa::Read) {
  Bossa *self = Nan::ObjectWrap::Unwrap<Bossa>(info.This());

  if (info.Length() != 3) {
    return Nan::ThrowTypeError("must provide offset, size and callback");
  }

  if (!info[0]->IsUint32()) {
    return Nan::ThrowTypeError("offset must be an integer");
  }
  uint32_t offset = Nan::To<uint32_t>(info[0]).FromJust();

  if (!info[1]->IsUint32()) {
    return Nan::ThrowTypeError("size must be an integer");
  }
  uint32_t size = Nan::To<uint32_t>(info[1]).FromJust();

  if (!info[2]->IsFunction()) {
    return Nan::ThrowTypeError("callback must be a function");
  }
  Nan::Callback *callback = new Nan::Callback(Nan::To<Function>(info[2]).ToLocalChecked());

  Nan::AsyncQueueWorker(new ReadWorker(callback, self, offset, size));
}

NAN_METHOD(Bossa::Erase) {
  Bossa *self = Nan::ObjectWrap::Unwrap<Bossa>(info.This());

  if (info.Length() != 2) {
    return Nan::ThrowTypeError("must provide offset and callback");
  }

  if (!info[0]->IsUint32()) {
    return Nan::ThrowTypeError("offset must be an integer");
  }
  uint32_t offset = Nan::To<uint32_t>(info[0]).FromJust();

  if (!info[1]->IsFunction()) {
    return Nan::ThrowTypeError("callback must be a function");
  }
  Nan::Callback *callback = new Nan::Callback(Nan::To<Function>(info[1]).ToLocalChecked());

  Nan::AsyncQueueWorker(new EraseWorker(callback, self, offset));
}

NAN_METHOD(Bossa::Verify) {
  Bossa *self = Nan::ObjectWrap::Unwrap<Bossa>(info.This());

  if (info.Length() != 3) {
    return Nan::ThrowTypeError("must provide buffer, offset and callback");
  }

  if (!info[0]->IsUint8Array()) {
    return Nan::ThrowTypeError("data must be a Buffer");
  }
  Local buffer = Nan::To<Object>(info[0]).ToLocalChecked();

  if (!info[1]->IsUint32()) {
    return Nan::ThrowTypeError("offset must be an integer");
  }
  uint32_t offset = Nan::To<uint32_t>(info[1]).FromJust();

  if (!info[2]->IsFunction()) {
    return Nan::ThrowTypeError("callback must be a function");
  }
  Nan::Callback *callback = new Nan::Callback(Nan::To<Function>(info[2]).ToLocalChecked());

  Nan::AsyncQueueWorker(new VerifyWorker(callback, self, buffer, offset));
}

NAN_METHOD(Bossa::Write) {
  Bossa *self = Nan::ObjectWrap::Unwrap<Bossa>(info.This());

  if (info.Length() != 3) {
    return Nan::ThrowTypeError("must provide buffer, offset and callback");
  }

  if (!info[0]->IsUint8Array()) {
    return Nan::ThrowTypeError("data must be a Buffer");
  }
  Local buffer = Nan::To<Object>(info[0]).ToLocalChecked();

  if (!info[1]->IsUint32()) {
    return Nan::ThrowTypeError("offset must be an integer");
  }
  uint32_t offset = Nan::To<uint32_t>(info[1]).FromJust();

  if (!info[2]->IsFunction()) {
    return Nan::ThrowTypeError("callback must be a function");
  }
  Nan::Callback *callback = new Nan::Callback(Nan::To<Function>(info[2]).ToLocalChecked());

  Nan::AsyncQueueWorker(new WriteWorker(callback, self, buffer, offset));
}
