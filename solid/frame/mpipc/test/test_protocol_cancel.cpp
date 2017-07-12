#include "solid/frame/mpipc/mpipcerror.hpp"
#include "solid/system/exception.hpp"
#include "test_protocol_common.hpp"

using namespace solid;

namespace {

struct InitStub {
    size_t                      size;
    bool                        cancel;
    frame::mpipc::MessageFlagsT flags;
};

InitStub initarray[] = {
    {100000, false, 0}, //first message must not be canceled
    {16384000, false, 0}, //not caceled
    {8192000, true, {frame::mpipc::MessageFlagsE::Synchronous}},
    {4096000, true, 0},
    {2048000, false, 0}, //not caceled
    {1024000, true, 0},
    {512000, false, {frame::mpipc::MessageFlagsE::Synchronous}}, //not canceled
    {256000, true, 0},
    {128000, true, 0},
    {64000, true, 0},
    {32000, false, 0}, //not canceled
    {16000, true, 0},
    {8000, true, 0},
    {4000, true, 0},
    {2000, true, 0},
};

typedef std::vector<frame::mpipc::MessageId> MessageIdVectorT;

std::string  pattern;
const size_t initarraysize = sizeof(initarray) / sizeof(InitStub);

size_t crtwriteidx = 0;
size_t crtreadidx  = 0;
size_t writecount  = 0;

MessageIdVectorT message_uid_vec;

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

using MessagePointerT = std::shared_ptr<Message>;

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
    if (_rerr and _rerr != frame::mpipc::error_message_canceled) {
        SOLID_THROW("Message complete with error");
    }
    if (_rmessage_ptr.get()) {
        size_t idx = static_cast<Message&>(*_rmessage_ptr).idx;
        if (crtreadidx) {
            //not the first message
            SOLID_CHECK((!_rerr and not initarray[idx % initarraysize].cancel) or (initarray[idx % initarraysize].cancel and _rerr == frame::mpipc::error_message_canceled));
        }
        idbg(static_cast<Message&>(*_rmessage_ptr).str.size() << ' ' << _rerr.message());
    }
    if (_rresponse_ptr.get()) {
        if (not static_cast<Message&>(*_rresponse_ptr).check()) {
            SOLID_THROW("Message check failed.");
        }

        ++crtreadidx;

        if (crtreadidx == 1) {
            idbg(crtreadidx << " canceling all messages");
            for (const auto& msguid : message_uid_vec) {

                frame::mpipc::MessageBundle msgbundle;
                frame::mpipc::MessageId     pool_msg_id;

                bool rv = ctx.mpipcmsgwriter->cancel(msguid, msgbundle, pool_msg_id);

                if (rv) {
                    idbg("Cancel message " << msguid << " retval = " << rv << " msgdatasz = " << static_cast<Message&>(*msgbundle.message_ptr).str.size());
                } else {
                    //TODO: add more checking
                }
            }
        } else {
            idbg(crtreadidx);
            size_t idx = static_cast<Message&>(*_rresponse_ptr).idx;
            SOLID_CHECK(not initarray[idx % initarraysize].cancel);
        }
    }
}

struct Receiver : frame::mpipc::MessageReader::Receiver {
    frame::mpipc::serialization_v1::Protocol&     rprotocol_;
    frame::mpipc::MessageWriter::RequestIdVectorT reqvec;
    uint8_t                                       ackd_count;

    Receiver(frame::mpipc::serialization_v1::Protocol& _rprotocol)
        : rprotocol_(_rprotocol)
        , ackd_count(15)
    {
    }

    void fillRequestVector(size_t _m = 10)
    {
        for (size_t i = 0; i < _m; ++i) {
            reqvec.push_back(frame::mpipc::RequestId(i, i));
        }
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

    void receiveAckCount(uint8_t _count) override
    {
        idbg("" << (int)_count);
        SOLID_CHECK(_count == ackd_count, "invalid ack count");
    }
    void receiveCancelRequest(const frame::mpipc::RequestId& _reqid) override
    {
        vdbg("" << _reqid);
        bool found = false;
        for (auto it = reqvec.cbegin(); it != reqvec.cend();) {
            if (*it == _reqid) {
                found = true;
                it    = reqvec.erase(it);
            } else {
                ++it;
            }
        }
        SOLID_CHECK(found, "request not found");
    }
};

} //namespace

int test_protocol_cancel(int argc, char** argv)
{

#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask("ew");
    Debug::the().moduleMask("any:view frame_mpipc:view");
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
    ErrorConditionT                   error;

    ctx.mpipcreaderconfig = &mpipcreaderconfig;
    ctx.mpipcwriterconfig = &mpipcwriterconfig;
    ctx.mpipcprotocol     = mpipcprotocol.get();
    ctx.mpipcmsgreader    = &mpipcmsgreader;
    ctx.mpipcmsgwriter    = &mpipcmsgwriter;

    mpipcwriterconfig.max_message_count_multiplex = 16;

    mpipcmsgwriter.prepare(mpipcwriterconfig);

    mpipcprotocol->registerType<::Message>(
        complete_message);

    const size_t start_count = 16;

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

        message_uid_vec.push_back(writer_msg_id);

        idbg("enqueue rv = " << rv << " writer_msg_id = " << writer_msg_id);
        SOLID_CHECK(rv);
        idbg(frame::mpipc::MessageWriterPrintPairT(mpipcmsgwriter, frame::mpipc::MessageWriter::PrintInnerListsE));

        if (not initarray[crtwriteidx % initarraysize].cancel) {
            idbg("do not cancel " << message_uid_vec.back());
            message_uid_vec.pop_back(); //we do not cancel this one
        }
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

        bool   is_running = true;
        size_t i          = 10;
        bool   refill     = false;

        while (is_running and !error) {
            refill = false;
            --rcvr.ackd_count;
            if (i) {
                --i;
                rcvr.fillRequestVector(10);
                refill = true;
            }

            uint32_t bufsz = mpipcmsgwriter.write(buf, bufcp, frame::mpipc::MessageWriter::WriteFlagsT(), rcvr.ackd_count, rcvr.reqvec, writercompletefnc, mpipcwriterconfig, *mpipcprotocol, mpipcconctx, error);

            if (refill) {
                rcvr.fillRequestVector(10);
            }

            if (!error and bufsz) {

                mpipcmsgreader.read(buf, bufsz, rcvr, mpipcreaderconfig, *mpipcprotocol, mpipcconctx, error);
            } else {
                is_running = false;
            }
        }
    }

    return 0;
}
