#include "solid/frame/mprpc/mprpcerror.hpp"
#include "solid/system/exception.hpp"
#include "test_protocol_common.hpp"
#include <iostream>

using namespace solid;

namespace {

struct InitStub {
    size_t                      size;
    bool                        cancel;
    frame::mprpc::MessageFlagsT flags;
};

InitStub initarray[] = {
    {100000, false, 0}, // first message must not be canceled
    {16384000, false, 0}, // not caceled
    {8192000, true, {frame::mprpc::MessageFlagsE::Synchronous}},
    {4096000, true, 0},
    {2048000, false, 0}, // not caceled
    {1024000, true, 0},
    {512000, false, {frame::mprpc::MessageFlagsE::Synchronous}}, // not canceled
    {256000, true, 0},
    {128000, true, 0},
    {64000, true, 0},
    {32000, false, 0}, // not canceled
    {16000, true, 0},
    {8000, true, 0},
    {4000, true, 0},
    {2000, true, 0},
};

typedef std::vector<frame::mprpc::MessageId> MessageIdVectorT;

std::string  pattern;
const size_t initarraysize = sizeof(initarray) / sizeof(InitStub);

uint32_t crtwriteidx = 0;
size_t   crtreadidx  = 0;
size_t   writecount  = 0;

MessageIdVectorT message_uid_vec;

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
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this << " idx = " << idx);
        init();
    }
    Message()
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this);
    }
    ~Message()
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << (void*)this);
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

using MessagePointerT = std::shared_ptr<Message>;

void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    frame::mprpc::MessagePointerT&   _rmessage_ptr,
    frame::mprpc::MessagePointerT&   _rresponse_ptr,
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
auto                             mprpcprotocol = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
    reflection::v1::metadata::factory,
    [](auto& _rmap) {
        _rmap.template registerMessage<Message>(1, "Message", complete_message);
    });

template <class ProtocolT>
struct Sender : frame::mprpc::MessageWriter::Sender {
    ProtocolT& rprotocol_;

    Sender(
        frame::mprpc::WriterConfiguration& _rconfig,
        ProtocolT&                         _rprotocol,
        frame::mprpc::ConnectionContext&   _conctx)
        : frame::mprpc::MessageWriter::Sender(_rconfig, _rprotocol, _conctx)
        , rprotocol_(_rprotocol)
    {
    }

    ErrorConditionT completeMessage(frame::mprpc::MessageBundle& _rmsgbundle, frame::mprpc::MessageId const& /*_rmsgid*/) override
    {
        solid_dbg(generic_logger, Info, "writer complete message");
        frame::mprpc::MessagePointerT response_ptr;
        ErrorConditionT               error;
        rprotocol_.complete(_rmsgbundle.message_type_id, mprpcconctx, _rmsgbundle.message_ptr, response_ptr, error);
        return ErrorConditionT();
    }

    bool cancelMessage(frame::mprpc::MessageBundle& _rmsgbundle, frame::mprpc::MessageId const& /*_rmsgid*/) override
    {
        solid_dbg(generic_logger, Info, "Cancel message " << static_cast<Message&>(*_rmsgbundle.message_ptr).str.size());
        return true;
    }
};

void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    frame::mprpc::MessagePointerT&   _rmessage_ptr,
    frame::mprpc::MessagePointerT&   _rresponse_ptr,
    ErrorConditionT const&           _rerr)
{
    if (_rerr && _rerr != frame::mprpc::error_message_canceled) {
        solid_throw("Message complete with error");
    }
    if (_rmessage_ptr.get()) {
        size_t idx = static_cast<Message&>(*_rmessage_ptr).idx;
        if (crtreadidx) {
            // not the first message
            solid_check((!_rerr && !initarray[idx % initarraysize].cancel) || (initarray[idx % initarraysize].cancel && _rerr == frame::mprpc::error_message_canceled));
        }
        solid_dbg(generic_logger, Info, static_cast<Message&>(*_rmessage_ptr).str.size() << ' ' << _rerr.message());
    }
    if (_rresponse_ptr.get()) {
        if (!static_cast<Message&>(*_rresponse_ptr).check()) {
            solid_throw("Message check failed.");
        }

        ++crtreadidx;

        if (crtreadidx == 1) {
            using ProtocolT = decltype(mprpcprotocol)::element_type;
            solid_dbg(generic_logger, Info, crtreadidx << " canceling all messages");
            for (const auto& msguid : message_uid_vec) {

                Sender<ProtocolT> sndr(*ctx.mprpcwriterconfig, *mprpcprotocol, mprpcconctx);

                ctx.mprpcmsgwriter->cancel(msguid, sndr);
            }
        } else {
            solid_dbg(generic_logger, Info, crtreadidx);
            size_t idx = static_cast<Message&>(*_rresponse_ptr).idx;
            solid_check(!initarray[idx % initarraysize].cancel);
        }
    }
}

template <class ProtocolT>
struct Receiver : frame::mprpc::MessageReader::Receiver {
    ProtocolT&                                    rprotocol_;
    frame::mprpc::MessageWriter::RequestIdVectorT reqvec;
    uint8_t                                       ackd_count;

    Receiver(frame::mprpc::ReaderConfiguration& _rconfig,
        ProtocolT&                              _rprotocol,
        frame::mprpc::ConnectionContext&        _conctx)
        : frame::mprpc::MessageReader::Receiver(_rconfig, _rprotocol, _conctx)
        , rprotocol_(_rprotocol)
        , ackd_count(15)
    {
    }

    void fillRequestVector(uint32_t _m = 10)
    {
        for (uint32_t i = 0; i < _m; ++i) {
            reqvec.push_back(frame::mprpc::RequestId(i, i));
        }
    }

    void receiveMessage(frame::mprpc::MessagePointerT& _rresponse_ptr, const size_t _msg_type_id) override
    {
        frame::mprpc::MessagePointerT message_ptr;
        ErrorConditionT               error;
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

} // namespace

int test_protocol_cancel(int argc, char* argv[])
{
    solid::log_start(std::cerr, {"solid::frame::mprpc.*:EWX", "\\*:EWX"});

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
    frame::mprpc::MessageReader       mprpcmsgreader;
    frame::mprpc::MessageWriter       mprpcmsgwriter;
    ErrorConditionT                   error;

    ctx.mprpcreaderconfig = &mprpcreaderconfig;
    ctx.mprpcwriterconfig = &mprpcwriterconfig;
    ctx.mprpcprotocol     = mprpcprotocol.get();
    ctx.mprpcmsgreader    = &mprpcmsgreader;
    ctx.mprpcmsgwriter    = &mprpcmsgwriter;

    mprpcwriterconfig.max_message_count_multiplex = 16;

    mprpcmsgwriter.prepare(mprpcwriterconfig);

    const size_t start_count = 16;

    writecount = 16; // start_count;//

    for (; crtwriteidx < start_count; ++crtwriteidx) {
        frame::mprpc::MessageBundle msgbundle;
        frame::mprpc::MessageId     writer_msg_id;
        frame::mprpc::MessageId     pool_msg_id;

        msgbundle.message_flags   = initarray[crtwriteidx % initarraysize].flags;
        msgbundle.message_ptr     = frame::mprpc::MessagePointerT(new Message(crtwriteidx));
        msgbundle.message_type_id = ctx.mprpcprotocol->typeIndex(msgbundle.message_ptr.get());

        bool rv = mprpcmsgwriter.enqueue(
            mprpcwriterconfig, msgbundle, pool_msg_id, writer_msg_id);

        message_uid_vec.push_back(writer_msg_id);

        solid_dbg(generic_logger, Info, "enqueue rv = " << rv << " writer_msg_id = " << writer_msg_id);
        solid_check(rv);
        solid_dbg(generic_logger, Info, frame::mprpc::MessageWriterPrintPairT(mprpcmsgwriter, frame::mprpc::MessageWriter::PrintInnerListsE));

        if (!initarray[crtwriteidx % initarraysize].cancel) {
            solid_dbg(generic_logger, Info, "do not cancel " << message_uid_vec.back());
            message_uid_vec.pop_back(); // we do not cancel this one
        }
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
