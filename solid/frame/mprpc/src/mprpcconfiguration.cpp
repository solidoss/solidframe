// solid/frame/ipc/src/ipcconfiguration.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "mprpcconnection.hpp"
#include "solid/frame/mprpc/mprpcerror.hpp"
#include "solid/frame/mprpc/mprpcsocketstub_plain.hpp"

#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/memory.hpp"

#include <cstring>

namespace solid {
namespace frame {
namespace mprpc {
//-----------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& _ros, const RelayDataFlagsT& _flags)
{
    bool b = false;
    if (_flags.has(RelayDataFlagsE::First)) {
        _ros << "First";
        b = true;
    }
    if (_flags.has(RelayDataFlagsE::Last)) {
        if (b)
            _ros << "|";
        _ros << "Last";
        b = true;
    }
    return _ros;
}

/*virtual*/ BufferBase::~BufferBase()
{
}

RecvBufferPointerT make_recv_buffer(const size_t _cp)
{
    switch (_cp) {
    case 512:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<512>>());
    case 1 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<1 * 1024>>());
    case 2 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<2 * 1024>>());
    case 4 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<4 * 1024>>());
    case 8 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<8 * 1024>>());
    case 16 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<16 * 1024>>());
    case 32 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<32 * 1024>>());
    case 64 * 1024:
        return std::dynamic_pointer_cast<BufferBase>(std::make_shared<Buffer<64 * 1024>>());
    default:
        return std::make_shared<Buffer<0>>(_cp);
    }
}

namespace {
RecvBufferPointerT default_allocate_recv_buffer(const uint32_t _cp)
{
    return make_recv_buffer(_cp);
}

SendBufferPointerT default_allocate_send_buffer(const uint32_t _cp)
{
    return SendBufferPointerT(new char[_cp]);
}

// void empty_reset_serializer_limits(ConnectionContext &, serialization::binary::Limits&){}

void empty_connection_stop(ConnectionContext&) {}

void empty_connection_start(ConnectionContext&) {}

void empty_connection_on_event(ConnectionContext&, EventBase&) {}

size_t default_compress(char*, size_t, ErrorConditionT&)
{
    return 0;
}

size_t default_decompress(char*, const char*, size_t, ErrorConditionT& _rerror)
{
    // This should never be called
    _rerror = error_compression_unavailable;
    return 0;
}

SocketStubPtrT default_create_client_socket(Configuration const& _rcfg, frame::aio::ActorProxy const& _rproxy, char* _emplace_buf)
{
    return plain::create_client_socket(_rcfg, _rproxy, _emplace_buf);
}

SocketStubPtrT default_create_server_socket(Configuration const& _rcfg, frame::aio::ActorProxy const& _rproxy, SocketDevice&& _usd, char* _emplace_buf)
{
    return plain::create_server_socket(_rcfg, _rproxy, std::move(_usd), _emplace_buf);
}

const char* default_extract_recipient_name(const char* _purl, std::string& _msgurl, std::string& _tmpstr)
{
    solid_assert_log(_purl != nullptr, service_logger());

    const char* const p = strchr(_purl, '/');

    if (p == nullptr) {
        return _purl;
    }
    _msgurl = (p + 1);
    _tmpstr.assign(_purl, p - _purl);
    return _tmpstr.c_str();
}

// const char* default_fast_extract_recipient_name(const char* _purl, std::string& _msgurl, std::string& _tmpstr)
// {
//
//     return _purl;
// }

bool default_setup_socket_device(SocketDevice& _rsd)
{
    _rsd.enableNoDelay();
    _rsd.enableNoSignal();
    return true;
}

} // namespace

ReaderConfiguration::ReaderConfiguration()
{
    max_message_count_multiplex = 64 + 128;

    decompress_fnc = &default_decompress;
}

WriterConfiguration::WriterConfiguration()
{
    max_message_count_multiplex = 64;

    max_message_continuous_packet_count = 4;
    max_message_count_response_wait     = 128;
    inplace_compress_fnc                = &default_compress;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/*static*/ RelayEngine& RelayEngine::instance()
{
    static RelayEngine eng;
    return eng;
}
//-----------------------------------------------------------------------------
bool RelayEngine::notifyConnection(Manager& _rm, const ActorIdT& _rrelay_uid, const RelayEngineNotification _what)
{
    return Connection::notify(_rm, _rrelay_uid, _what);
}
//-----------------------------------------------------------------------------
/*virtual*/ RelayEngine::~RelayEngine()
{
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngine::stopConnection(const UniqueId& /*_rrelay_uid*/)
{
}
//-----------------------------------------------------------------------------
/*virtual*/ bool RelayEngine::doRelayStart(
    const ActorIdT& /*_rcon_uid*/,
    UniqueId& /*_rrelay_uid*/,
    MessageHeader& /*_rmsghdr*/,
    RelayData&& /*_urelay_data*/,
    MessageId& /*_rrelay_id*/,
    ErrorConditionT& /*_rerror*/)
{
    return false;
}
//-----------------------------------------------------------------------------
/*virtual*/ bool RelayEngine::doRelayResponse(
    const UniqueId& /*_rrelay_uid*/,
    MessageHeader& /*_rmsghdr*/,
    RelayData&& /*_urelay_data*/,
    const MessageId& /*_rrelay_id*/,
    ErrorConditionT& /*_rerror*/)
{
    return false;
}
//-----------------------------------------------------------------------------
/*virtual*/ bool RelayEngine::doRelay(
    const UniqueId& /*_rrelay_uid*/,
    RelayData&& /*_urelay_data*/,
    const MessageId& /*_rrelay_id*/,
    ErrorConditionT& /*_rerror*/)
{
    return false;
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngine::doComplete(
    const UniqueId& /*_rrelay_uid*/,
    RelayData* /*_prelay_data*/,
    MessageId const& /*_rengine_msg_id*/,
    bool& /*_rmore*/
)
{
    solid_throw_log(service_logger(), "should not be called");
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngine::doCancel(
    const UniqueId& /*_rrelay_uid*/,
    RelayData* /*_prelay_data*/,
    MessageId const& /*_rengine_msg_id*/,
    DoneFunctionT& /*_done_fnc*/
)
{
    solid_throw_log(service_logger(), "should not be called");
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngine::doPollNew(const UniqueId& /*_rrelay_uid*/, PushFunctionT& /*_try_push_fnc*/, bool& /*_rmore*/)
{
    solid_throw_log(service_logger(), "should not be called");
}
//-----------------------------------------------------------------------------
/*virtual*/ void RelayEngine::doPollDone(const UniqueId& /*_rrelay_uid*/, DoneFunctionT& /*_done_fnc*/, CancelFunctionT& /*_cancel_fnc*/)
{
    solid_throw_log(service_logger(), "should not be called");
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Configuration::init()
{
    connection_recv_buffer_start_capacity_kb = static_cast<uint8_t>(memory_page_size() / 1024);
    connection_send_buffer_start_capacity_kb = static_cast<uint8_t>(memory_page_size() / 1024);

    connection_recv_buffer_max_capacity_kb = connection_send_buffer_max_capacity_kb = 64;

    connection_recv_buffer_allocate_fnc = &default_allocate_recv_buffer;
    connection_send_buffer_allocate_fnc = &default_allocate_send_buffer;

    connection_stop_fnc = &empty_connection_stop;

    server.connection_start_fnc = &empty_connection_start;
    client.connection_start_fnc = &empty_connection_start;

    connection_on_event_fnc = &empty_connection_on_event;

    client.connection_create_socket_fnc = &default_create_client_socket;
    server.connection_create_socket_fnc = &default_create_server_socket;

    server.socket_device_setup_fnc = &default_setup_socket_device;
    client.socket_device_setup_fnc = &default_setup_socket_device;

    extract_recipient_name_fnc = &default_extract_recipient_name;

    pool_max_active_connection_count  = 1;
    pool_max_pending_connection_count = 1;
    pool_max_message_queue_size       = 1024;
    relay_enabled                     = false;
}
//-----------------------------------------------------------------------------
std::chrono::milliseconds Configuration::Client::connectionReconnectTimeout(
    const uint8_t _retry_count,
    const bool    _failed_create_connection_actor,
    const bool    _last_connection_was_connected,
    const bool    _last_connection_was_active,
    const bool /*_last_connection_was_secured*/) const
{
    static const uint16_t entropy[] = {
        839, 771, 683, 826, 950, 315, 240, 394, 369, 230, 684, 887, 588, 766, 112, 793, 541, 295, 297, 450,
        355, 601, 562, 331, 699, 958, 948, 333, 435, 872, 964, 748, 723, 715, 358, 158, 233, 321, 833, 624,
        736, 357, 236, 565, 553, 347, 830, 886, 422, 605, 725, 426, 417, 519, 174, 603, 511, 452, 711, 892,
        910, 310, 985, 299, 584, 458, 667, 765, 110, 136, 385, 806, 442, 906, 534, 365, 392, 254, 913, 518,
        135, 138, 520, 206, 995, 145, 161, 438, 202, 348, 430, 491, 652, 680, 743, 957, 876, 235, 567, 787,
        187, 228, 148, 104, 656, 147, 207, 577, 437, 926, 499, 535, 738, 884, 575, 655, 401, 127, 125, 932,
        735, 791, 445, 854, 380, 396, 700, 739, 762, 425, 959, 155, 890, 263, 853, 822, 982, 322, 694, 397,
        277, 378, 143, 836, 772, 808, 324, 823, 740, 287, 563, 659, 850, 673, 709, 313, 523, 232, 858, 221,
        997, 925, 866, 737, 151, 895, 679, 509, 914, 372, 265, 539, 406, 537, 472, 177, 500, 978, 363, 970,
        328, 778, 260, 761, 843, 379, 439, 176, 157, 441, 341, 123, 330, 962, 488, 448, 807, 528, 371, 391,
        209, 915, 862, 253, 609, 204, 292, 317, 264, 336, 797, 185, 785, 413, 179, 857, 825, 946, 666, 477,
        690, 688, 213, 343, 644, 769, 156, 691, 345, 966, 216, 227, 917, 482, 693, 507, 984, 149, 864, 718,
        767, 431, 183, 996, 436, 909, 669, 821, 591, 109, 237, 867, 803, 581, 730, 171, 225, 616, 750, 653,
        552, 122, 899, 924, 390, 305, 424, 707, 319, 597, 897, 542, 354, 941, 662, 754, 474, 593, 576, 218,
        712, 682, 751, 269, 326, 904, 689, 164, 649, 606, 630, 200, 845, 121, 387, 560, 963, 585, 182, 252,
        126, 492, 753, 353, 359, 124, 891, 153, 981, 976, 568, 628, 804, 794, 189, 302, 774, 980, 800, 828,
        459, 998, 993, 208, 525, 169, 154, 752, 223, 325, 405, 168, 288, 243, 956, 764, 880, 217, 988, 728,
        613, 247, 457, 918, 255, 856, 133, 898, 544, 922, 453, 779, 165, 554, 979, 134, 831, 398, 759, 933,
        524, 626, 619, 463, 621, 882, 911, 670, 526, 246, 971, 768, 327, 590, 889, 604, 637, 529, 473, 638,
        543, 466, 610, 349, 654, 665, 215, 625, 549, 163, 574, 298, 639, 798, 533, 493, 773, 219, 645, 847,
        432, 697, 120, 195, 501, 818, 434, 947, 954, 635, 173, 308, 191, 226, 564, 211, 994, 802, 494, 279,
        146, 113, 651, 270, 991, 366, 777, 538, 282, 530, 943, 367, 301, 580, 714, 271, 551, 607, 536, 902,
        512, 393, 783, 128, 780, 478, 196, 819, 361, 955, 602, 722, 192, 893, 629, 548, 557, 627, 381, 527,
        532, 376, 703, 285, 814, 281, 199, 300, 676, 938, 868, 987, 796, 342, 114, 650, 632, 953, 589, 888,
        510, 351, 464, 258, 744, 571, 283, 461, 960, 573, 289, 935, 874, 280, 940, 640, 834, 181, 657, 731,
        713, 784, 454, 166, 160, 100, 824, 234, 309, 758, 175, 641, 968, 356, 578, 412, 210, 273, 224, 763,
        587, 375, 198, 250, 844, 479, 377, 119, 274, 643, 307, 846, 861, 972, 514, 159, 496, 851, 555, 696,
        303, 801, 267, 671, 261, 726, 180, 487, 642, 170, 675, 931, 415, 407, 873, 286, 451, 414, 318, 178,
        842, 410, 908, 486, 710, 685, 951, 975, 720, 965, 504, 276, 745, 885, 776, 266, 374, 949, 660, 433,
        799, 600, 334, 513, 420, 907, 320, 849, 484, 746, 559, 633, 719, 505, 446, 792, 786, 340, 101, 142,
        939, 674, 952, 942, 102, 841, 813, 428, 262, 810, 469, 314, 596, 423, 790, 167, 470, 193, 384, 983,
        517, 382, 388, 515, 306, 335, 920, 475, 623, 259, 755, 934, 837, 678, 231, 130, 419, 708, 672, 141,
        595, 859, 188, 418, 404, 239, 108, 927, 506, 238, 545, 476, 634, 468, 883, 402, 618, 990, 244, 107,
        455, 829, 471, 677, 661, 408, 815, 284, 212, 594, 485, 203, 197, 516, 131, 395, 338, 974, 827, 304,
        118, 111, 592, 546, 973, 921, 205, 249, 757, 362, 617, 400, 241, 944, 928, 172, 373, 695, 460, 860,
        443, 194, 444, 903, 701, 352, 386, 572, 323, 805, 316, 346, 698, 916, 999, 360, 612, 611, 832, 481,
        498, 137, 788, 409, 522, 871, 579, 781, 989, 312, 152, 811, 566, 558, 870, 116, 705, 912, 268, 756,
        865, 681, 508, 490, 140, 370, 364, 875, 550, 296, 741, 201, 937, 569, 782, 399, 540, 835, 704, 692,
        598, 615, 467, 840, 105, 291, 547, 789, 139, 919, 992, 495, 848, 294, 242, 256, 184, 570, 290, 368,
        531, 332, 222, 961, 770, 350, 447, 727, 275, 608, 809, 812, 214, 729, 293, 427, 429, 881, 462, 747,
        521, 749, 664, 586, 775, 838, 162, 967, 311, 896, 855, 329, 936, 923, 190, 599, 879, 668, 117, 894,
        717, 456, 817, 724, 561, 186, 229, 636, 421, 945, 877, 583, 337, 663, 278, 339, 863, 106, 760, 614,
        969, 251, 411, 502, 702, 622, 795, 930, 497, 272, 220, 706, 344, 686, 103, 449, 503, 901, 383, 483,
        245, 905, 480, 150, 977, 440, 248, 929, 687, 129, 648, 646, 816, 620, 716, 820, 733, 465, 115, 878,
        144, 869, 389, 582, 416, 257, 986, 852, 132, 403, 556, 658, 489, 734, 721, 647, 742, 732, 900, 631};
    static std::atomic<uint16_t> crtidx{0};

    if (_failed_create_connection_actor) {
        return connection_timeout_reconnect;
    }
    if (_last_connection_was_active || _last_connection_was_connected) {
        return std::chrono::milliseconds(entropy[crtidx.fetch_add(1) % std::size(entropy)]); // reconnect right away
    }

    std::chrono::milliseconds amount = (connection_timeout_reconnect / connection_reconnect_steps) * _retry_count;

    if (amount > connection_timeout_reconnect) {
        amount = connection_timeout_reconnect;
    }

    return amount + std::chrono::milliseconds(entropy[crtidx.fetch_add(1) % std::size(entropy)]);
}
//-----------------------------------------------------------------------------
void Configuration::check() const
{
}
//-----------------------------------------------------------------------------
void Configuration::prepare()
{

    if (pool_max_active_connection_count == 0) {
        pool_max_active_connection_count = 1;
    }
    if (pool_max_pending_connection_count == 0) {
        pool_max_pending_connection_count = 1;
    }

    if (connection_recv_buffer_max_capacity_kb > 64) {
        connection_recv_buffer_max_capacity_kb = 64;
    }

    if (connection_send_buffer_max_capacity_kb > 64) {
        connection_send_buffer_max_capacity_kb = 64;
    }

    if (connection_recv_buffer_start_capacity_kb > connection_recv_buffer_max_capacity_kb) {
        connection_recv_buffer_start_capacity_kb = connection_recv_buffer_max_capacity_kb;
    }

    if (connection_send_buffer_start_capacity_kb > connection_send_buffer_max_capacity_kb) {
        connection_send_buffer_start_capacity_kb = connection_send_buffer_max_capacity_kb;
    }

    if (!server.hasSecureConfiguration()) {
        server.connection_start_secure = false;
    }
    if (!client.hasSecureConfiguration()) {
        client.connection_start_secure = false;
    }
    if (pool_count < pool_mutex_count) {
        pool_mutex_count = pool_count;
    }
}

//-----------------------------------------------------------------------------

void Configuration::createListenerDevice(SocketDevice& _rsd) const
{
    if (!server.listener_address_str.empty()) {
        std::string tmp;
        const char* hst_name;
        const char* svc_name;

        size_t off = server.listener_address_str.rfind(':');
        if (off != std::string::npos) {
            tmp      = server.listener_address_str.substr(0, off);
            hst_name = tmp.c_str();
            svc_name = server.listener_address_str.c_str() + off + 1;
            if (svc_name[0] == 0) {
                svc_name = server.listener_service_str.c_str();
            }
        } else {
            hst_name = server.listener_address_str.c_str();
            svc_name = server.listener_service_str.c_str();
        }

        ResolveData rd = synchronous_resolve(hst_name, svc_name, 0, -1, SocketInfo::Stream);

        for (auto it = rd.begin(); it != rd.end(); ++it) {
            SocketDevice sd;
            sd.create(it);
            const auto err = sd.prepareAccept(it, SocketInfo::max_listen_backlog_size());
            if (!err) {
                _rsd = std::move(sd);
                return; // SUCCESS
            }
        }

        solid_throw_log(service_logger(), "failed to create listener socket device");
    }
}

//-----------------------------------------------------------------------------
RecvBufferPointerT Configuration::allocateRecvBuffer(uint8_t& _rbuffer_capacity_kb) const
{
    if (_rbuffer_capacity_kb == 0) {
        _rbuffer_capacity_kb = connection_recv_buffer_start_capacity_kb;
    } else if (_rbuffer_capacity_kb > connection_recv_buffer_max_capacity_kb) {
        _rbuffer_capacity_kb = connection_recv_buffer_max_capacity_kb;
    }
    return connection_recv_buffer_allocate_fnc(_rbuffer_capacity_kb * 1024);
}
//-----------------------------------------------------------------------------
SendBufferPointerT Configuration::allocateSendBuffer(uint8_t& _rbuffer_capacity_kb) const
{
    if (_rbuffer_capacity_kb == 0) {
        _rbuffer_capacity_kb = connection_send_buffer_start_capacity_kb;
    } else if (_rbuffer_capacity_kb > connection_send_buffer_max_capacity_kb) {
        _rbuffer_capacity_kb = connection_send_buffer_max_capacity_kb;
    }
    return connection_send_buffer_allocate_fnc(_rbuffer_capacity_kb * 1024);
}
//-----------------------------------------------------------------------------
} // namespace mprpc
} // namespace frame
} // namespace solid
