#include "solid/system/exception.hpp"
#include "test_protocol_common.hpp"
#include <iostream>

using namespace solid;

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

size_t   crtwriteidx = 0;
uint32_t crtreadidx  = 0;
size_t   writecount  = 0;

size_t real_size(size_t _sz)
{
    // offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

struct Message : frame::mprpc::Message {
    uint32_t    idx;
    std::string str;

    Message(uint32_t _idx)
        : idx(_idx)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << this << " idx = " << idx);
        init();
    }
    Message()
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << this);
    }
    ~Message()
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << this);
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.idx, _rctx, 0, "idx").add(_rthis.str, _rctx, 1, "str");
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
            pu[i] = pup[i % pattern_size]; // pattern[i % pattern.size()];
        }
    }
    bool check() const
    {
        const size_t sz = real_size(initarray[idx % initarraysize].size);
        solid_dbg(generic_logger, Info, "str.size = " << str.size() << " should be equal to " << sz);
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

using MessagePointerT = solid::frame::mprpc::MessagePointerT<Message>;

void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    MessagePointerT&                 _rmessage_ptr,
    MessagePointerT&                 _rresponse_ptr,
    ErrorConditionT const&           _rerr);

struct Context {
    Context()
        : mprpcreaderconfig(nullptr)
        , mprpcwriterconfig(nullptr)
        , mprpcprotocol(nullptr)
        , mprpcmsgreader(nullptr)
        , mprpcmsgwriter(nullptr)
    {
    }

    frame::mprpc::ReaderConfiguration* mprpcreaderconfig;
    frame::mprpc::WriterConfiguration* mprpcwriterconfig;
    frame::mprpc::Protocol*            mprpcprotocol;
    frame::mprpc::MessageReader*       mprpcmsgreader;
    frame::mprpc::MessageWriter*       mprpcmsgwriter;

} ctx;

frame::mprpc::ConnectionContext& mprpcconctx(frame::mprpc::TestEntryway::createContext());

void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    MessagePointerT&                 _rmessage_ptr,
    MessagePointerT&                 _rresponse_ptr,
    ErrorConditionT const&           _rerr)
{
    if (_rerr) {
        solid_throw("Message complete with error");
    }
    if (_rmessage_ptr.get()) {
        solid_dbg(generic_logger, Info, static_cast<Message*>(_rmessage_ptr.get())->idx);
    }

    if (_rresponse_ptr.get()) {

        if (!static_cast<Message&>(*_rresponse_ptr).check()) {
            solid_throw("Message check failed.");
        }

        ++crtreadidx;
        solid_dbg(generic_logger, Info, crtreadidx);

        if (crtwriteidx < writecount) {
            frame::mprpc::MessageBundle msgbundle;
            frame::mprpc::MessageId     writer_msg_id;
            frame::mprpc::MessageId     pool_msg_id;

            msgbundle.message_flags   = initarray[crtwriteidx % initarraysize].flags;
            msgbundle.message_ptr     = frame::mprpc::make_message<Message>(crtwriteidx);
            msgbundle.message_type_id = ctx.mprpcprotocol->typeIndex(msgbundle.message_ptr.get());

            bool rv = ctx.mprpcmsgwriter->enqueue(
                *ctx.mprpcwriterconfig, msgbundle, pool_msg_id, writer_msg_id);

            solid_dbg(generic_logger, Info, "enqueue rv = " << rv << " writer_msg_id = " << writer_msg_id);
            solid_dbg(generic_logger, Info, frame::mprpc::MessageWriterPrintPairT(*ctx.mprpcmsgwriter, frame::mprpc::MessageWriter::PrintInnerListsE));
            ++crtwriteidx;
        }
    }
}

template <class ProtocolT>
struct Receiver : frame::mprpc::MessageReaderReceiver {
    ProtocolT&                                    rprotocol_;
    frame::mprpc::MessageWriter::RequestIdVectorT reqvec;
    uint8_t                                       ackd_count;

    Receiver(frame::mprpc::ReaderConfiguration& _rconfig,
        ProtocolT&                              _rprotocol,
        frame::mprpc::ConnectionContext&        _conctx)
        : frame::mprpc::MessageReaderReceiver(_rconfig, _rprotocol, _conctx)
        , rprotocol_(_rprotocol)
        , ackd_count(15)
    {
    }

    void fillRequestVector(size_t _m = 10)
    {
        for (size_t i = 0; i < _m; ++i) {
            reqvec.push_back(frame::mprpc::RequestId(i, i));
        }
    }

    void receiveMessage(frame::mprpc::MessagePointerT<>& _rresponse_ptr, const size_t _msg_type_id) override
    {
        frame::mprpc::MessagePointerT<> message_ptr;
        ErrorConditionT                 error;
        rprotocol_.complete(_msg_type_id, mprpcconctx, message_ptr, _rresponse_ptr, error);
    }

    void receiveKeepAlive() override
    {
        solid_dbg(generic_logger, Info, "");
    }

    void receiveAckCount(uint8_t _count) override
    {
        solid_dbg(generic_logger, Info, "" << (int)_count);
        solid_check(_count == ackd_count, "invalid ack count");
    }
    void receiveCancelRequest(const frame::mprpc::RequestId& _reqid) override
    {
        solid_dbg(generic_logger, Verbose, "" << _reqid);
        bool found = false;
        for (auto it = reqvec.cbegin(); it != reqvec.cend();) {
            if (*it == _reqid) {
                found = true;
                it    = reqvec.erase(it);
            } else {
                ++it;
            }
        }
        solid_check(found, "request not found");
    }
};

template <class ProtocolT>
struct Sender : frame::mprpc::MessageWriterSender {
    ProtocolT& rprotocol_;

    Sender(
        frame::mprpc::WriterConfiguration& _rconfig,
        ProtocolT&                         _rprotocol,
        frame::mprpc::ConnectionContext&   _conctx)
        : frame::mprpc::MessageWriterSender(_rconfig, _rprotocol, _conctx)
        , rprotocol_(_rprotocol)
    {
    }

    ErrorConditionT completeMessage(frame::mprpc::MessageBundle& _rmsgbundle, frame::mprpc::MessageId const& /*_rmsgid*/) override
    {
        solid_dbg(generic_logger, Info, "writer complete message");
        frame::mprpc::MessagePointerT<> response_ptr;
        ErrorConditionT                 error;
        rprotocol_.complete(_rmsgbundle.message_type_id, mprpcconctx, _rmsgbundle.message_ptr, response_ptr, error);
        return ErrorConditionT();
    }
};

} // namespace

int test_protocol_basic(int argc, char* argv[])
{

    solid::log_start(std::cerr, {".*:EWX"});

    for (int i = 0; i < 127; ++i) {
        if (isprint(i) && !isblank(i)) {
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

    frame::mprpc::WriterConfiguration mprpcwriterconfig;
    frame::mprpc::ReaderConfiguration mprpcreaderconfig;
    auto                              mprpcprotocol = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
        reflection::v1::metadata::factory,
        [&](auto& _rmap) {
            _rmap.template registerMessage<Message>(1, "Message", complete_message);
        });
    frame::mprpc::MessageReader mprpcmsgreader;
    frame::mprpc::MessageWriter mprpcmsgwriter;

    ErrorConditionT error;

    ctx.mprpcreaderconfig = &mprpcreaderconfig;
    ctx.mprpcwriterconfig = &mprpcwriterconfig;
    ctx.mprpcprotocol     = mprpcprotocol.get();
    ctx.mprpcmsgreader    = &mprpcmsgreader;
    ctx.mprpcmsgwriter    = &mprpcmsgwriter;

    mprpcmsgwriter.prepare(mprpcwriterconfig);

    const size_t start_count = 10;

    writecount = 10 * initarraysize; // start_count;//

    for (; crtwriteidx < start_count; ++crtwriteidx) {
        frame::mprpc::MessageBundle msgbundle;
        frame::mprpc::MessageId     writer_msg_id;
        frame::mprpc::MessageId     pool_msg_id;

        msgbundle.message_flags   = initarray[crtwriteidx % initarraysize].flags;
        msgbundle.message_ptr     = MessagePointerT(frame::mprpc::make_message<Message>(crtwriteidx));
        msgbundle.message_type_id = ctx.mprpcprotocol->typeIndex(msgbundle.message_ptr.get());

        bool rv = mprpcmsgwriter.enqueue(
            mprpcwriterconfig, msgbundle, pool_msg_id, writer_msg_id);
        solid_dbg(generic_logger, Info, "enqueue rv = " << rv << " writer_msg_id = " << writer_msg_id);
        solid_dbg(generic_logger, Info, frame::mprpc::MessageWriterPrintPairT(mprpcmsgwriter, frame::mprpc::MessageWriter::PrintInnerListsE));
    }

    {
        using ProtocolT = decltype(mprpcprotocol)::element_type;
        Receiver<ProtocolT> rcvr(mprpcreaderconfig, *mprpcprotocol, mprpcconctx);
        Sender<ProtocolT>   sndr(mprpcwriterconfig, *mprpcprotocol, mprpcconctx);

        mprpcmsgreader.prepare(mprpcreaderconfig);

        bool   is_running = true;
        size_t i          = 10;
        bool   refill     = false;

        while (is_running && !error) {
            refill = false;
            --rcvr.ackd_count;
            if (i) {
                --i;
                rcvr.fillRequestVector(10);
                refill = true;
            }
            frame::mprpc::WriteBuffer wb(buf, bufcp);
            uint8_t                   relay_free_count = 0;
            uint8_t                   ack_cnt          = rcvr.ackd_count;
            error                                      = mprpcmsgwriter.write(wb, frame::mprpc::MessageWriter::WriteFlagsT(), ack_cnt, rcvr.reqvec, relay_free_count, sndr);
            if (refill) {
                rcvr.fillRequestVector(10);
            }
            if (!error && wb.size()) {

                mprpcmsgreader.read(wb.data(), wb.size(), rcvr, error);
            } else {
                is_running = false;
            }
        }
    }
    return 0;
}
