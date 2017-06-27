#include "solid/system/exception.hpp"
#include "test_protocol_common.hpp"

using namespace solid;

namespace {

struct InitStub {
    size_t size;
    ulong  flags;
};

InitStub initarray[] = {
    {100000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {16384000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {8192000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {4096000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {2048000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {1024000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {512000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {256000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {128000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {64000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {32000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {16000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {8000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {4000, 0 | frame::mpipc::MessageFlags::Synchronous},
    {2000, 0 | frame::mpipc::MessageFlags::Synchronous},
};

std::string  pattern;
const size_t initarraysize = sizeof(initarray) / sizeof(InitStub);

size_t crtwriteidx = 0;
size_t crtreadidx  = 0;
size_t writecount  = 0;

size_t real_size(size_t _sz)
{
    //offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

struct Message : frame::mpipc::Message {
    uint32_t    idx;
    std::string str;

    Message(uint32_t _idx)
        : idx(_idx)
    {
        idbg("CREATE ---------------- " << (void*)this << " idx = " << idx);
        init();
    }
    Message()
    {
        idbg("CREATE ---------------- " << (void*)this);
    }
    ~Message()
    {
        idbg("DELETE ---------------- " << (void*)this);
    }

    template <class S>
    void solidSerialize(S& _s, frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(str, "str");
        _s.push(idx, "idx");
    }

    void init()
    {
        const size_t sz = real_size(initarray[idx % initarraysize].size);
        str.resize(sz);
        const size_t    count        = sz / sizeof(uint64_t);
        uint64_t*       pu           = reinterpret_cast<uint64_t*>(const_cast<char*>(str.data()));
        const uint64_t* pup          = reinterpret_cast<const uint64_t*>(pattern.data());
        const size_t    pattern_size = pattern.size() / sizeof(uint64_t);
        for (uint64_t i = 0; i < count; ++i) {
            pu[i] = pup[i % pattern_size]; //pattern[i % pattern.size()];
        }
    }
    bool check() const
    {
        const size_t sz = real_size(initarray[idx % initarraysize].size);
        idbg("str.size = " << str.size() << " should be equal to " << sz);
        if (sz != str.size()) {
            return false;
        }
        const size_t    count        = sz / sizeof(uint64_t);
        const uint64_t* pu           = reinterpret_cast<const uint64_t*>(str.data());
        const uint64_t* pup          = reinterpret_cast<const uint64_t*>(pattern.data());
        const size_t    pattern_size = pattern.size() / sizeof(uint64_t);

        for (uint64_t i = 0; i < count; ++i) {
            if (pu[i] != pup[i % pattern_size])
                return false;
        }
        return true;
    }
};

void complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    frame::mpipc::MessagePointerT&   _rmessage_ptr,
    frame::mpipc::MessagePointerT&   _rresponse_ptr,
    ErrorConditionT const&           _rerr);

struct Context {
    Context()
        : mpipcreaderconfig(nullptr)
        , mpipcwriterconfig(nullptr)
        , mpipcprotocol(nullptr)
        , mpipcmsgreader(nullptr)
        , mpipcmsgwriter(nullptr)
    {
    }

    frame::mpipc::ReaderConfiguration* mpipcreaderconfig;
    frame::mpipc::WriterConfiguration* mpipcwriterconfig;
    frame::mpipc::Protocol*            mpipcprotocol;
    frame::mpipc::MessageReader*       mpipcmsgreader;
    frame::mpipc::MessageWriter*       mpipcmsgwriter;

} ctx;

frame::mpipc::ConnectionContext& mpipcconctx(frame::mpipc::TestEntryway::createContext());

void complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    frame::mpipc::MessagePointerT&   _rmessage_ptr,
    frame::mpipc::MessagePointerT&   _rresponse_ptr,
    ErrorConditionT const&           _rerr)
{
    if (_rerr) {
        SOLID_THROW("Message complete with error");
    }
    if (_rmessage_ptr.get()) {
        idbg(static_cast<Message*>(_rmessage_ptr.get())->idx);
    }

    if (_rresponse_ptr.get()) {

        size_t msgidx = static_cast<Message&>(*_rresponse_ptr).idx;

        if (not static_cast<Message&>(*_rresponse_ptr).check()) {
            SOLID_THROW("Message check failed.");
        }

        if (msgidx != crtreadidx) {
            SOLID_THROW("Message index invalid - SynchronousFlagE failed.");
        }

        ++crtreadidx;

        idbg(crtreadidx);

        if (crtwriteidx < writecount) {
            frame::mpipc::MessageBundle msgbundle;
            frame::mpipc::MessageId     writer_msg_id;
            frame::mpipc::MessageId     pool_msg_id;

            msgbundle.message_flags   = initarray[crtwriteidx % initarraysize].flags;
            msgbundle.message_ptr     = frame::mpipc::MessagePointerT(new Message(crtwriteidx));
            msgbundle.message_type_id = ctx.mpipcprotocol->typeIndex(msgbundle.message_ptr.get());

            bool rv = ctx.mpipcmsgwriter->enqueue(
                *ctx.mpipcwriterconfig, msgbundle, pool_msg_id, writer_msg_id);

            idbg("enqueue rv = " << rv << " writer_msg_id = " << writer_msg_id);
            idbg(frame::mpipc::MessageWriterPrintPairT(*ctx.mpipcmsgwriter, frame::mpipc::MessageWriter::PrintInnerListsE));
            ++crtwriteidx;
        }
    }
}

struct Receiver : frame::mpipc::MessageReader::Receiver {
    frame::mpipc::serialization_v1::Protocol& rprotocol_;

    Receiver(frame::mpipc::serialization_v1::Protocol& _rprotocol)
        : rprotocol_(_rprotocol)
    {
    }

    void receiveMessage(frame::mpipc::MessagePointerT& _rresponse_ptr, const size_t _msg_type_id) override
    {
        frame::mpipc::MessagePointerT message_ptr;
        ErrorConditionT               error;
        rprotocol_[_msg_type_id].complete_fnc(mpipcconctx, message_ptr, _rresponse_ptr, error);
    }

    void receiveKeepAlive() override
    {
        idbg("");
    }
};

} //namespace

int test_protocol_synchronous(int argc, char** argv)
{

#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask("ew");
    Debug::the().moduleMask("any:ew");
    Debug::the().initStdErr(false, nullptr);
#endif

    for (int i = 0; i < 127; ++i) {
        if (isprint(i) and !isblank(i)) {
            pattern += static_cast<char>(i);
        }
    }

    size_t sz = real_size(pattern.size());

    if (sz > pattern.size()) {
        pattern.resize(sz - sizeof(uint64_t));
    } else if (sz < pattern.size()) {
        pattern.resize(sz);
    }

    const uint16_t bufcp(1024 * 4);
    char           buf[bufcp];

    frame::mpipc::WriterConfiguration mpipcwriterconfig;
    frame::mpipc::ReaderConfiguration mpipcreaderconfig;
    auto                              mpipcprotocol = frame::mpipc::serialization_v1::Protocol::create();
    frame::mpipc::MessageReader       mpipcmsgreader;
    frame::mpipc::MessageWriter       mpipcmsgwriter;

    ErrorConditionT error;

    ctx.mpipcreaderconfig = &mpipcreaderconfig;
    ctx.mpipcwriterconfig = &mpipcwriterconfig;
    ctx.mpipcprotocol     = mpipcprotocol.get();
    ctx.mpipcmsgreader    = &mpipcmsgreader;
    ctx.mpipcmsgwriter    = &mpipcmsgwriter;

    mpipcmsgwriter.prepare(mpipcwriterconfig);

    mpipcprotocol->registerType<::Message>(
        complete_message);

    const size_t start_count = 10;

    writecount = 16; //start_count;//

    for (; crtwriteidx < start_count; ++crtwriteidx) {
        frame::mpipc::MessageBundle msgbundle;
        frame::mpipc::MessageId     writer_msg_id;
        frame::mpipc::MessageId     pool_msg_id;

        msgbundle.message_flags   = initarray[crtwriteidx % initarraysize].flags;
        msgbundle.message_ptr     = frame::mpipc::MessagePointerT(new Message(crtwriteidx));
        msgbundle.message_type_id = ctx.mpipcprotocol->typeIndex(msgbundle.message_ptr.get());

        bool rv = mpipcmsgwriter.enqueue(
            mpipcwriterconfig, msgbundle, pool_msg_id, writer_msg_id);
        SOLID_CHECK(rv);
        idbg("enqueue rv = " << rv << " writer_msg_id = " << writer_msg_id);
        idbg(frame::mpipc::MessageWriterPrintPairT(mpipcmsgwriter, frame::mpipc::MessageWriter::PrintInnerListsE));
    }

    {
        Receiver rcvr(*mpipcprotocol);

        auto writer_complete_lambda(
            [&mpipcprotocol](frame::mpipc::MessageBundle& _rmsgbundle, frame::mpipc::MessageId const& _rmsgid) {
                idbg("writer complete message");
                frame::mpipc::MessagePointerT response_ptr;
                ErrorConditionT               error;
                (*mpipcprotocol)[_rmsgbundle.message_type_id].complete_fnc(mpipcconctx, _rmsgbundle.message_ptr, response_ptr, error);
                return ErrorConditionT();
            });

        frame::mpipc::MessageWriter::CompleteFunctionT writercompletefnc(std::cref(writer_complete_lambda));

        mpipcmsgreader.prepare(mpipcreaderconfig);

        bool is_running = true;

        while (is_running and !error) {
            uint32_t bufsz = mpipcmsgwriter.write(buf, bufcp, false, writercompletefnc, mpipcwriterconfig, *mpipcprotocol, mpipcconctx, error);

            if (!error and bufsz) {

                mpipcmsgreader.read(buf, bufsz, rcvr, mpipcreaderconfig, *mpipcprotocol, mpipcconctx, error);
            } else {
                is_running = false;
            }
        }
    }

    return 0;
}
