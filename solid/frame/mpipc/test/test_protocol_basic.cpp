#include "solid/system/exception.hpp"
#include "test_protocol_common.hpp"

using namespace solid;

using ProtocolT = frame::mpipc::serialization_v2::Protocol<uint8_t>;

namespace {

struct InitStub {
    size_t size;
    ulong  flags;
};

// InitStub initarray[] = {
//  {16384000, 0},
//  {8192000, 0},
//  {4096000, 0},
//  {2048000, 0},
//  {1024000, 0},
//  {512000, 0},
//  {256000, 0},
//  {128000, 0},
//  {64000, 0},
//  {32000, 0},
//  {16000, 0},
//  {8000, 0},
//  {4000, 0},
//  {2000, 0},
// };

InitStub initarray[] = {
    {100000, 0},
    {2000, 0},
    {4000, 0},
    {8000, 0},
    {16000, 0},
    {32000, 0},
    {64000, 0},
    {128000, 0},
    {256000, 0},
    {512000, 0},
    {1024000, 0},
    {2048000, 0},
    {4096000, 0},
    {8192000, 0},
    {16384000, 0}};

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

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name){
        _s.add(_rthis.str, _rctx, "str").add(_rthis.idx, _rctx, "idx");
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

        if (not static_cast<Message&>(*_rresponse_ptr).check()) {
            SOLID_THROW("Message check failed.");
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
    ProtocolT&     rprotocol_;
    frame::mpipc::MessageWriter::RequestIdVectorT reqvec;
    uint8_t                                       ackd_count;

    Receiver(frame::mpipc::ReaderConfiguration&   _rconfig,
        ProtocolT& _rprotocol,
        frame::mpipc::ConnectionContext&          _conctx)
        : frame::mpipc::MessageReader::Receiver(_rconfig, _rprotocol, _conctx)
        , rprotocol_(_rprotocol)
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
        rprotocol_.complete(_msg_type_id, mpipcconctx, message_ptr, _rresponse_ptr, error);
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

struct Sender : frame::mpipc::MessageWriter::Sender {
    ProtocolT& rprotocol_;

    Sender(
        frame::mpipc::WriterConfiguration&        _rconfig,
        ProtocolT& _rprotocol,
        frame::mpipc::ConnectionContext&          _conctx)
        : frame::mpipc::MessageWriter::Sender(_rconfig, _rprotocol, _conctx)
        , rprotocol_(_rprotocol)
    {
    }

    ErrorConditionT completeMessage(frame::mpipc::MessageBundle& _rmsgbundle, frame::mpipc::MessageId const& /*_rmsgid*/) override
    {
        idbg("writer complete message");
        frame::mpipc::MessagePointerT response_ptr;
        ErrorConditionT               error;
        rprotocol_.complete(_rmsgbundle.message_type_id, mpipcconctx, _rmsgbundle.message_ptr, response_ptr, error);
        return ErrorConditionT();
    }
};

} //namespace

int test_protocol_basic(int argc, char** argv)
{

#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask("ew");
    Debug::the().moduleMask("any");
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
    auto                              mpipcprotocol = ProtocolT::create();
    frame::mpipc::MessageReader       mpipcmsgreader;
    frame::mpipc::MessageWriter       mpipcmsgwriter;

    ErrorConditionT error;

    ctx.mpipcreaderconfig = &mpipcreaderconfig;
    ctx.mpipcwriterconfig = &mpipcwriterconfig;
    ctx.mpipcprotocol     = mpipcprotocol.get();
    ctx.mpipcmsgreader    = &mpipcmsgreader;
    ctx.mpipcmsgwriter    = &mpipcmsgwriter;

    mpipcmsgwriter.prepare(mpipcwriterconfig);
    
    mpipcprotocol->null(0);
    mpipcprotocol->registerMessage<::Message>(complete_message, 1);

    const size_t start_count = 10;

    writecount = 10 * initarraysize; //start_count;//

    for (; crtwriteidx < start_count; ++crtwriteidx) {
        frame::mpipc::MessageBundle msgbundle;
        frame::mpipc::MessageId     writer_msg_id;
        frame::mpipc::MessageId     pool_msg_id;

        msgbundle.message_flags   = initarray[crtwriteidx % initarraysize].flags;
        msgbundle.message_ptr     = frame::mpipc::MessagePointerT(new Message(crtwriteidx));
        msgbundle.message_type_id = ctx.mpipcprotocol->typeIndex(msgbundle.message_ptr.get());

        bool rv = mpipcmsgwriter.enqueue(
            mpipcwriterconfig, msgbundle, pool_msg_id, writer_msg_id);
        idbg("enqueue rv = " << rv << " writer_msg_id = " << writer_msg_id);
        idbg(frame::mpipc::MessageWriterPrintPairT(mpipcmsgwriter, frame::mpipc::MessageWriter::PrintInnerListsE));
    }

    {
        Receiver rcvr(mpipcreaderconfig, *mpipcprotocol, mpipcconctx);
        Sender   sndr(mpipcwriterconfig, *mpipcprotocol, mpipcconctx);

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
            frame::mpipc::WriteBuffer wb(buf, bufcp);
            uint8_t                   relay_free_count = 0;
            uint8_t                   ack_cnt          = rcvr.ackd_count;
            error                                      = mpipcmsgwriter.write(wb, frame::mpipc::MessageWriter::WriteFlagsT(), ack_cnt, rcvr.reqvec, relay_free_count, sndr);
            if (refill) {
                rcvr.fillRequestVector(10);
            }
            if (!error and wb.size()) {

                mpipcmsgreader.read(wb.data(), wb.size(), rcvr, error);
            } else {
                is_running = false;
            }
        }
    }
    return 0;
}
