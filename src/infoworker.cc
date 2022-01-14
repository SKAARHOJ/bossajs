#include "infoworker.h"
#include "util.h"

using namespace v8;


void InfoWorker::Execute() {
    try {
        bossa->info(finfo);
    } catch(const std::exception& exc) {
        SetErrorMessage(exc.what());
    }
}


void InfoWorker::HandleOKCallback() {
    Nan::HandleScope scope;

    Local<Object> obj = Nan::New<Object>();
    Nan::Set(obj, L("name"), L(finfo.name));
    Nan::Set(obj, L("chipId"), Nan::New(finfo.chipId));
    Nan::Set(obj, L("extChipId"), Nan::New(finfo.extChipId));
    Nan::Set(obj, L("version"), L(finfo.version));
    Nan::Set(obj, L("address"), Nan::New(finfo.address));
    Nan::Set(obj, L("numPages"), Nan::New(finfo.numPages));
    Nan::Set(obj, L("pageSize"), Nan::New(finfo.pageSize));
    Nan::Set(obj, L("totalSize"), Nan::New(finfo.totalSize));
    Nan::Set(obj, L("numPlanes"), Nan::New(finfo.numPlanes));

    Nan::Set(obj, L("security"), Nan::New(finfo.security));
    Nan::Set(obj, L("bootFlash"), Nan::New(finfo.bootFlash));
    Nan::Set(obj, L("bod"), Nan::New(finfo.bod));
    Nan::Set(obj, L("bor"), Nan::New(finfo.bor));

    Nan::Set(obj, L("canBootFlash"), Nan::New(finfo.canBootFlash));
    Nan::Set(obj, L("canBod"), Nan::New(finfo.canBod));
    Nan::Set(obj, L("canBor"), Nan::New(finfo.canBor));
    Nan::Set(obj, L("canChipErase"), Nan::New(finfo.canChipErase));
    Nan::Set(obj, L("canWriteBuffer"), Nan::New(finfo.canWriteBuffer));
    Nan::Set(obj, L("canChecksumBuffer"), Nan::New(finfo.canChecksumBuffer));

    Local<Array> lockRegions = Nan::New<Array>(finfo.lockRegions.size());
    unsigned index = 0;
    for (auto elem : finfo.lockRegions) {
        Nan::Set(lockRegions, index, Nan::New(elem));
        index++;
    }
    Nan::Set(obj, L("lockRegions"), lockRegions);

    Local<Value> argv[] = {
        Nan::Null(),  // err
        obj,  // value
    };

    callback->Call(2, argv, async_resource);
}


void InfoWorker::HandleErrorCallback() {
    Nan::HandleScope scope;

    Local<Value> argv[] = {
        Nan::Error(this->ErrorMessage()),  // err
        Nan::Null(),
    };

    callback->Call(2, argv, async_resource);
}
