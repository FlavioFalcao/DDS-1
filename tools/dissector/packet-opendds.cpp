/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "tools/dissector/packet-opendds.h"

#include <ace/Basic_Types.h>
#include <ace/CDR_Base.h>
#include <ace/Message_Block.h>
#include <ace/Log_Msg.h>
#include <ace/ACE.h>

#include <cstring>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

//using namespace OpenDDS::DCPS;

// value ABSOLUTE_TIME_LOCAL, 1.2.x does not.  This technique uses
// the ABSOLUTE_TIME_LOCAL value if it is present (1.3.x),
// and uses BASE_NONE if it is not (1.2.x).  This must be in
// the same scope as the Wireshark 1.2.x declaration of
// the ABSOLUTE_TIME_LOCAL enum value, which is why it is in the
// global namespace.
struct ABSOLUTE_TIME_LOCAL {
  static const int value = BASE_NONE;
};

namespace {

// These two functions are the rest of the
// Wireshark 1.2.x / 1.3.x compatibility solution.
template <int V> int enum_value() { return V; }
template <typename T> int enum_value() { return T::value; }

int proto_opendds    = -1;

gboolean opendds_desegment = TRUE;
const char * DCPS_MAGIC = "DCPS";
dissector_handle_t dcps_tcp_handle;

int hf_version    = -1;
int hf_length     = -1;
int hf_sequence   = -1;
int hf_source     = -1;

int hf_flags               = -1;
int hf_flags_byte_order    = -1;
int hf_flags_first_fragment= -1;
int hf_flags_last_fragment = -1;

const int flags_bits = 8;
const int* flags_fields[] = {
  &hf_flags_byte_order,
  &hf_flags_first_fragment,
  &hf_flags_last_fragment,
  NULL
};


int hf_sample             = -1;
int hf_sample_id          = -1;
int hf_sample_sub_id      = -1;
int hf_sample_length      = -1;
int hf_sample_sequence    = -1;
int hf_sample_timestamp   = -1;
int hf_sample_lifespan    = -1;
int hf_sample_publication = -1;
int hf_sample_publisher   = -1;
int hf_sample_content_filt= -1;

int hf_sample_flags             = -1;
int hf_sample_flags_byte_order  = -1;
int hf_sample_flags_coherent    = -1;
int hf_sample_flags_historic    = -1;
int hf_sample_flags_lifespan    = -1;
int hf_sample_flags_group_coh   = -1;
int hf_sample_flags_content_filt= -1;
int hf_sample_flags_seq_repair  = -1;
int hf_sample_flags_more_frags  = -1;

const int sample_flags_bits = 8;
const int* sample_flags_fields[] = {
  &hf_sample_flags_byte_order,
  &hf_sample_flags_coherent,
  &hf_sample_flags_historic,
  &hf_sample_flags_lifespan,
  &hf_sample_flags_group_coh,
  &hf_sample_flags_content_filt,
  &hf_sample_flags_seq_repair,
  &hf_sample_flags_more_frags,
  NULL
};

gint ett_trans_header  = -1;
gint ett_trans_flags   = -1;
gint ett_sample_header = -1;
gint ett_sample_flags  = -1;

const value_string byte_order_vals[] = {
  { 0x0,  "Big Endian"    },
  { 0x1,  "Little Endian" },
  { 0,    NULL            }
};

const true_false_string byte_order_tfs = {
  "Little Endian",
  "Big Endian"
};

const value_string sample_id_vals[] = {
  { OpenDDS::DCPS::SAMPLE_DATA,            "SAMPLE_DATA"           },
  { OpenDDS::DCPS::DATAWRITER_LIVELINESS,  "DATAWRITER_LIVELINESS" },
  { OpenDDS::DCPS::INSTANCE_REGISTRATION,  "INSTANCE_REGISTRATION" },
  { OpenDDS::DCPS::UNREGISTER_INSTANCE,    "UNREGISTER_INSTANCE"   },
  { OpenDDS::DCPS::DISPOSE_INSTANCE,       "DISPOSE_INSTANCE"      },
  { OpenDDS::DCPS::GRACEFUL_DISCONNECT,    "GRACEFUL_DISCONNECT"   },
  { OpenDDS::DCPS::FULLY_ASSOCIATED,       "FULLY_ASSOCIATED"      },
  { OpenDDS::DCPS::REQUEST_ACK,            "REQUEST_ACK"           },
  { OpenDDS::DCPS::SAMPLE_ACK,             "SAMPLE_ACK"            },
  { OpenDDS::DCPS::END_COHERENT_CHANGES,   "END_COHERENT_CHANGES"  },
  { OpenDDS::DCPS::TRANSPORT_CONTROL,      "TRANSPORT_CONTROL"     },
  { 0,                                     NULL                    }
};

const value_string sample_sub_id_vals[] = {
  { OpenDDS::DCPS::SUBMESSAGE_NONE,        "SUBMESSAGE_NONE"       },
  { OpenDDS::DCPS::MULTICAST_SYN,          "MULTICAST_SYN"         },
  { OpenDDS::DCPS::MULTICAST_SYNACK,       "MULTICAST_SYNACK"      },
  { OpenDDS::DCPS::MULTICAST_NAK,          "MULTICAST_NAK"         },
  { OpenDDS::DCPS::MULTICAST_NAKACK,       "MULTICAST_NAKACK"      },
  { 0,                                     NULL                    }
};

extern "C"
dissector_Export void
dissect_opendds (tvbuff_t* tvb, packet_info* pinfo, proto_tree* tree);

} // namespace

namespace OpenDDS
{

  Sample_Dissector_Base::Sample_Dissector_Base (const char *type_id)
    :typeId_ (0),
     publication_()
  {
    size_t len = ACE_OS::strlen(type_id);
    if (len > 0)
      {
        this->typeId_ = new char[len+1];
        ACE_OS::strcpy (this->typeId_, type_id);
      }
  }

  Sample_Dissector_Base::~Sample_Dissector_Base ()
  {
    delete [] this->typeId_;
  }

  const char *
  Sample_Dissector_Base::typeId() const
  {
    return this->typeId_;
  }

  const DCPS::RepoId&
  Sample_Dissector_Base::publication () const
  {
    return this->publication_;
  }

  void
  Sample_Dissector_Base::publication (const DCPS::RepoId& pub)
  {
    ACE_OS::memcpy (&this->publication_, &pub, sizeof(DCPS::RepoId));
  }

  void
  Sample_Dissector_Manager::add (Sample_Dissector_Base &d)
  {
#if 0

    dds_sub_handle_t *subh;

    subh = g_malloc(sizeof (giop_sub_handle_t));

    subh->sub_name = name;
    subh->sub_fn = sub;
    subh->sub_proto = find_protocol_by_id(sub_proto);     /* protocol_t for sub dissectors's proto_register_protocol() */

    giop_sub_list = g_slist_append (giop_sub_list, subh);
#endif

    const char *key = d.typeId();
    ACE_DEBUG ((LM_DEBUG,"Adding new dissector for %s\n",key));

    dissectors_.bind(key,&d);
  }

  template<typename T>
  T
  demarshal_data(tvbuff_t* tvb, gint offset)
  {
    T t;

    guint len = std::min(tvb->length - offset,
                         static_cast<guint>(t.max_marshaled_size()));
    const guint8* data = tvb_get_ptr(tvb, offset, len);

    ACE_Message_Block mb(reinterpret_cast<const char*>(data));
    mb.wr_ptr(len);

    t.init(&mb);

    return t;
  }

  std::string
  DCPS_DDS_Dissector::format (const DCPS::TransportHeader& header)
  {
    std::ostringstream os;

    os << "Length: " << std::dec << header.length_;
    if (header.first_fragment_) os << ", First Fragment";
    if (header.last_fragment_) os << ", Last Fragment";
    os << ", Sequence: 0x" << std::hex << std::setw(8) << std::setfill('0')
       << header.sequence_.getValue();
    os << ", Source: 0x" << std::hex << std::setw(8) << std::setfill('0')
       << ACE_UINT32(header.source_);

    return os.str();
  }

  std::string
  DCPS_DDS_Dissector::format (const DCPS::DataSampleHeader& sample)
  {
    std::ostringstream os;

    if (sample.submessage_id_ != DCPS::SUBMESSAGE_NONE) {
      os << DCPS::SubMessageId(sample.submessage_id_)
         << " (0x" << std::hex << std::setw(2) << std::setfill('0')
         << unsigned(sample.submessage_id_) << ")";
    } else {
      os << DCPS::MessageId(sample.message_id_)
         << " (0x" << std::hex << std::setw(2) << std::setfill('0')
         << unsigned(sample.message_id_) << ")";
    }

    os << ", Length: " << std::dec << sample.message_length_;

    if (sample.message_id_ != DCPS::TRANSPORT_CONTROL) {
      if (sample.message_id_ == DCPS::SAMPLE_DATA) {
        os << ", Sequence: 0x" << std::hex << std::setw(8) << std::setfill('0')
           << sample.sequence_.getValue();
      }
      os << ", Publication: " << DCPS::RepoIdConverter(sample.publication_id_);
    }

    return os.str();
  }


  void
  DCPS_DDS_Dissector::dissect_transport_header(proto_tree* tree,
                                               const DCPS::TransportHeader& header,
                                               gint& offset)
  {
    gint len;

    offset += sizeof(header.protocol_) - 2; // skip preamble

    // hf_version
    len = sizeof(header.protocol_) - offset;
    proto_tree_add_bytes_format_value(tree, hf_version, tvb, offset, len,
                                      reinterpret_cast<const guint8*>(header.protocol_ + offset),
                                      "%d.%d", header.protocol_[4], header.protocol_[5]);
    offset += len;

    // hf_flags
    len = sizeof(ACE_CDR::Octet);
    proto_tree_add_bitmask(tree, tvb, offset, hf_flags, ett_trans_flags,
                           flags_fields, FALSE);
    offset += len;

    offset += sizeof(header.reserved_);     // skip reserved

    // hf_length
    len = sizeof(header.length_);
    proto_tree_add_uint_format_value(tree, hf_length, tvb, offset, len,
                                     header.length_, "%d octets", header.length_);
    offset += len;

    // hf_sequence
    len = static_cast<gint>(gen_find_size(header.sequence_));
    proto_tree_add_uint64(tree, hf_sequence, tvb, offset, len,
                          gint64(header.sequence_.getValue()));
    offset += len;

    // hf_source
    len = sizeof(header.source_);
    proto_tree_add_uint(tree, hf_source, tvb, offset, len,
                        guint32(header.source_));
    offset += len;
  }

  void
  DCPS_DDS_Dissector::dissect_sample_header (proto_tree* tree,
                                             const DCPS::DataSampleHeader& sample,
                                             gint& offset)
  {
    gint len;

    // hf_sample_id
    len = sizeof(sample.message_id_);
    proto_tree_add_item(tree, hf_sample_id, tvb, offset, len, FALSE);
    offset += len;

    // hf_sample_sub_id
    len = sizeof(sample.submessage_id_);
    if (sample.submessage_id_ != DCPS::SUBMESSAGE_NONE) {
      proto_tree_add_item(tree, hf_sample_sub_id, tvb, offset, len, FALSE);
    }
    offset += len;
    // hf_sample_flags
    len = sizeof(ACE_CDR::Octet);
    proto_tree_add_bitmask(tree, tvb, offset,
                           hf_sample_flags, ett_sample_flags, sample_flags_fields, FALSE);
    offset += len;

    // hf_sample_length
    len = sizeof(sample.message_length_);
    proto_tree_add_uint_format_value(tree, hf_sample_length, tvb, offset, len,
                                     sample.message_length_, "%d octets", sample.message_length_);
    offset += len;

    // hf_sample_sequence
    len = static_cast<gint>(gen_find_size(sample.sequence_));
    if (sample.message_id_ == DCPS::SAMPLE_DATA) {
      proto_tree_add_uint64(tree, hf_sample_sequence, tvb, offset, len,
                            gint64(sample.sequence_.getValue()));
    }
    offset += len;

    // hf_sample_timestamp
    len = sizeof(sample.source_timestamp_sec_) +
      sizeof(sample.source_timestamp_nanosec_);
    if (sample.message_id_ != DCPS::TRANSPORT_CONTROL)
      {
        nstime_t ns =
          {
            sample.source_timestamp_sec_,
            sample.source_timestamp_nanosec_
          };
        proto_tree_add_time(tree, hf_sample_timestamp, tvb, offset, len, &ns);
      }
    offset += len;

    // hf_sample_lifespan
    if (sample.lifespan_duration_) {
      len = sizeof(sample.lifespan_duration_sec_) +
        sizeof(sample.lifespan_duration_nanosec_);
      if (sample.message_id_ != DCPS::TRANSPORT_CONTROL) {
        nstime_t ns = {
          sample.lifespan_duration_sec_,
          sample.lifespan_duration_nanosec_
        };
        proto_tree_add_time(tree, hf_sample_lifespan, tvb, offset, len, &ns);
      }
      offset += len;
    }

    // hf_sample_publication
    len = static_cast<gint>(gen_find_size(sample.publication_id_));
    if (sample.message_id_ != DCPS::TRANSPORT_CONTROL) {
      DCPS::RepoIdConverter converter (sample.publication_id_);
      proto_tree_add_bytes_format_value
        (tree,
         hf_sample_publication,
         tvb, offset,
         len,
         reinterpret_cast<const guint8*>(&sample.publication_id_),
         "%s",
         std::string(converter).c_str()
         );
    }
    offset += len;

    // hf_sample_publisher
    if (sample.group_coherent_) {
      len = static_cast<gint>(gen_find_size(sample.publisher_id_));
      if (sample.message_id_ != DCPS::TRANSPORT_CONTROL) {
        DCPS::RepoIdConverter converter(sample.publisher_id_);
        proto_tree_add_bytes_format_value
          (tree,
           hf_sample_publisher,
           tvb,
           offset,
           len,
           reinterpret_cast<const guint8*>(&sample.publisher_id_),
           "%s",
           std::string(converter).c_str()
           );
      }
      offset += len;
    }

    // hf_sample_content_filt
    if (sample.content_filter_) {
      gint total_len =
        static_cast<gint>(gen_find_size(sample.content_filter_entries_));
      len = sizeof(CORBA::ULong);
      if (sample.message_id_ != DCPS::TRANSPORT_CONTROL) {
        proto_tree_add_uint_format_value(tree, hf_sample_content_filt, tvb,
                                         offset, len,
                                         sample.content_filter_entries_.length(),
                                         "%d entries",
                                         sample.content_filter_entries_.length());
      }
      offset += total_len;
      //TODO: represent the content_filter entries in wireshark, for now skip
    }

    offset += sample.message_length_; // skip marshaled data
  }



  void
  DCPS_DDS_Dissector::dissect ()
  {

    gint offset = 0;

    if (check_col(pinfo->cinfo, COL_PROTOCOL)) {
      col_set_str(pinfo->cinfo, COL_PROTOCOL, "OpenDDS");
    }

    if (check_col(pinfo->cinfo, COL_INFO)) {
      col_clear(pinfo->cinfo, COL_INFO);
    }

    DCPS::TransportHeader trans =
      demarshal_data<DCPS::TransportHeader>(tvb, offset);
    std::string trans_str(format(trans));

    if (check_col(pinfo->cinfo, COL_INFO)) {
      col_add_fstr(pinfo->cinfo, COL_INFO, "DCPS %s", trans_str.c_str());
    }

    if (tree != NULL) {
      proto_item* item =
        proto_tree_add_protocol_format(tree,
                                       proto_opendds, tvb, 0, -1,
                                       "OpenDDS DCPS Protocol, %s",
                                       trans_str.c_str());

      proto_tree* trans_tree = proto_item_add_subtree(item, ett_trans_header);

      this->dissect_transport_header (trans_tree, trans, offset);

      while (offset < gint(tvb->length))
        {
          DCPS::DataSampleHeader sample =
            demarshal_data<DCPS::DataSampleHeader>(tvb, offset);

          std::string sample_str(format(sample));

          if (sample.message_id_ == DCPS::SAMPLE_DATA)
            {
              ACE_DEBUG ((LM_DEBUG, "sample data: is %d, len = %d\n",
                          offset, tvb->length));
            }

          proto_item* item =
            proto_tree_add_none_format
            (trans_tree, hf_sample, tvb, offset,
             static_cast<gint>(sample.marshaled_size()) +
             sample.message_length_,
             "%s",
             sample_str.c_str()
             );

          proto_tree* sample_tree = proto_item_add_subtree(item, ett_sample_header);

          this-> dissect_sample_header (sample_tree, sample, offset);
      }
    }
  }

  bool
  DCPS_DDS_Dissector::dissect_heur()
  {
    gint len = sizeof(DCPS::TransportHeader::DCPS_PROTOCOL);
    guint8* data = ::tvb_get_ephemeral_string(tvb, 0, len);

    if (std::memcmp(data, DCPS::TransportHeader::DCPS_PROTOCOL, len) != 0)
      {
        return false;
      }

    if ( pinfo->ptype == PT_TCP )
      {
        // A converstion is used to keep track of a series of frames
        // that carry data between a connected pair of TCP endpoints.

        if (!pinfo->fd->flags.visited)
          {
            // adapted this from the implementation of
            // find_or_create_converation which was not available prior to
            // 1.4.x wireshark.
            conversation_t *conv =
              ::find_conversation(pinfo->fd->num, &pinfo->src, &pinfo->dst,
                                  pinfo->ptype, pinfo->srcport,
                                  pinfo->destport, 0);
            if (conv == 0)
              {
                // this is a new conversation
                conv = ::conversation_new(pinfo->fd->num, &pinfo->src,
                                          &pinfo->dst, pinfo->ptype,
                                          pinfo->srcport, pinfo->destport, 0);
              }
            ::conversation_set_dissector(conv, dcps_tcp_handle);
          }

        ::dissect_opendds (tvb, pinfo, tree);
      }
    else
      {
        this->dissect ();
      }


    ::dissect_opendds(tvb, pinfo, tree);
    return true;
  }
}

// function passed to tcp pdu parser for computing packet length
extern "C"
guint
get_dcps_pdu_len(packet_info *, tvbuff_t *tvb, int offset)
{
  if ( tvb_memeql(tvb, 0, reinterpret_cast<const guint8*>(DCPS_MAGIC) ,4) != 0)
    return 0;

  OpenDDS::DCPS::TransportHeader header =
    OpenDDS::demarshal_data<OpenDDS::DCPS::TransportHeader>(tvb, offset);

  return header.length_ + static_cast<guint>(header.max_marshaled_size());
}

// function passed to tcp pdu parser to do actual work.
extern "C"
dissector_Export void
dissect_dcps_common(tvbuff_t* tvb, packet_info* pinfo, proto_tree* tree)
{
  OpenDDS::DCPS_DDS_Dissector dissector(tvb, pinfo, tree);
  dissector.dissect();
}

extern "C"
dissector_Export void
dissect_opendds (tvbuff_t* tvb, packet_info* pinfo, proto_tree* tree)
{
  // this can filter between DCPS and RTPS as needed.
  tcp_dissect_pdus(tvb,
                   pinfo,
                   tree,
                   opendds_desegment,
                   sizeof (OpenDDS::DCPS::TransportHeader),
                   get_dcps_pdu_len,
                   dissect_dcps_common);
}
#if 0
extern "C"
dissector_Export gboolean
dissect_inforepo_giop(tvbuff_t *tvb, packet_info *pinfo,
                      proto_tree *ptree, int *offset,
                      ::MessageHeader *header, gchar *operation,
                      gchar *idlname)
{
  ACE_DEBUG ((LM_DEBUG,"pinfo.protocol = %s\n", pinfo->current_proto));
  if (idlname == 0)
    return FALSE;
  OpenDDS::DCPS_DDS_Dissector dissector (tvb, pinfo, ptree);
  *offset += dissector.dissect_giop (header, operation, idlname);
  return TRUE;
}

extern "C"
dissector_Export gboolean
dissect_inforepo_giop_heur (tvbuff_t *tvb, packet_info *pinfo,
                            proto_tree *ptree, int *offset,
                            ::MessageHeader *header, gchar *operation, gchar *mod)
{
  ACE_DEBUG ((LM_DEBUG,"heur pinfo.protocol = %s\n", pinfo->current_proto));
  return FALSE;
#if 0
  OpenDDS::DCPS_DDS_Dissector dissector (tvb, pinfo, ptree);
  *offset += dissector.dissect_giop (header, operation, idlname);
  return true;
#endif

}
#endif

extern "C"
dissector_Export gboolean
dissect_opendds_heur(tvbuff_t* tvb, packet_info* pinfo, proto_tree* tree)
{
  OpenDDS::DCPS_DDS_Dissector dissector(tvb, pinfo, tree);

  return dissector.dissect_heur();
}

extern "C"
dissector_Export void
proto_register_opendds()
{

  //OpenDDS::DCPS_DDS_Dissector::init ();

  //#define HFILL 0, 0, HF_REF_TYPE_NONE, 0, NULL, NULL
#define NULL_HFILL NULL, 0, NULL, HFILL
#define BF_HFILL(X) NULL, 1 << X, NULL, HFILL

  static hf_register_info hf[] = {
    { &hf_version,
      { "Version", "opendds.version", FT_BYTES, BASE_NONE, NULL_HFILL
      }
    },
    { &hf_flags,
      { "Flags", "opendds.flags", FT_UINT8, BASE_HEX, NULL_HFILL
      }
    },
    { &hf_flags_byte_order,
      { "Byte order", "opendds.flags.byte_order",
        FT_BOOLEAN, flags_bits, TFS(&byte_order_tfs), 1 << 0, NULL, HFILL
      }
    },
    { &hf_flags_first_fragment,
      { "First fragment", "opendds.flags.first_fragment",
        FT_BOOLEAN, flags_bits, BF_HFILL(1)
      }
    },
    { &hf_flags_last_fragment,
      { "Last fragment", "opendds.flags.last_fragment",
        FT_BOOLEAN, flags_bits, BF_HFILL(2)
      }
    },
    { &hf_length,
      { "Length", "opendds.length",
        FT_UINT16, BASE_HEX, NULL_HFILL
      }
    },
    { &hf_sequence,
      { "Sequence", "opendds.sequence",
        FT_UINT64, BASE_HEX, NULL_HFILL
      }
    },
    { &hf_source,
      { "Source", "opendds.source",
        FT_UINT32, BASE_HEX, NULL_HFILL
      }
    },
    { &hf_sample,
      { "Sample", "opendds.sample",
        FT_NONE, BASE_NONE, NULL_HFILL
      }
    },
    { &hf_sample_id,
      { "ID", "opendds.sample.id",
        FT_UINT8, BASE_HEX, VALS(sample_id_vals), 0, NULL, HFILL
      }
    },
    { &hf_sample_sub_id,
      { "Sub-ID", "opendds.sample.sub_id",
        FT_UINT8, BASE_HEX, VALS(sample_sub_id_vals), 0, NULL, HFILL
      }
    },
    { &hf_sample_flags,
      { "Flags", "opendds.sample.flags",
        FT_UINT8, BASE_HEX, NULL_HFILL
      }
    },
    { &hf_sample_flags_byte_order,
      { "Byte order", "opendds.sample.flags.byte_order",
        FT_BOOLEAN, sample_flags_bits,
        TFS(&byte_order_tfs), 1 << 0, NULL, HFILL
      }
    },
    { &hf_sample_flags_coherent,
      { "Coherent", "opendds.sample.flags.coherent",
        FT_BOOLEAN, sample_flags_bits, BF_HFILL (1)
      }
    },
    { &hf_sample_flags_historic,
      { "Historic", "opendds.sample.flags.historic",
          FT_BOOLEAN, sample_flags_bits, BF_HFILL(2)
      }
    },
    { &hf_sample_flags_lifespan,
      { "Lifespan", "opendds.sample.flags.lifespan",
          FT_BOOLEAN, sample_flags_bits, BF_HFILL(3)
      }
    },
    { &hf_sample_flags_group_coh,
      { "Group Coherent", "opendds.sample.flags.group_coherent",
          FT_BOOLEAN, sample_flags_bits, BF_HFILL(4)
      }
    },
    { &hf_sample_flags_content_filt,
      { "Content Filtering", "opendds.sample.flags.content_filter",
          FT_BOOLEAN, sample_flags_bits, BF_HFILL(5)
      }
    },
    { &hf_sample_flags_seq_repair,
      { "Sequence Repair", "opendds.sample.flags.sequence_repair",
          FT_BOOLEAN, sample_flags_bits, BF_HFILL(6)
      }
    },
    { &hf_sample_flags_more_frags,
      { "More Fragments", "opendds.sample.flags.more_frags",
          FT_BOOLEAN, sample_flags_bits, BF_HFILL(7)
      }
    },
    { &hf_sample_length,
      { "Length", "opendds.sample.length",
        FT_UINT32, BASE_HEX, NULL_HFILL
      }
    },
    { &hf_sample_sequence,
      { "Sequence", "opendds.sample.sequence",
        FT_UINT64, BASE_HEX, NULL_HFILL
      }
    },
    { &hf_sample_timestamp,
      { "Timestamp", "opendds.sample.timestamp",
        FT_ABSOLUTE_TIME, enum_value<ABSOLUTE_TIME_LOCAL>(), NULL_HFILL
      }
    },
    { &hf_sample_lifespan,
      { "Lifespan", "opendds.sample.lifespan",
        FT_RELATIVE_TIME, BASE_NONE, NULL_HFILL
      }
    },
    { &hf_sample_publication,
      { "Publication", "opendds.sample.publication",
        FT_BYTES, BASE_NONE, NULL_HFILL
      }
    },
    { &hf_sample_publisher,
      { "Publisher", "opendds.sample.publisher",
        FT_BYTES, BASE_NONE, NULL_HFILL
      }
    },
    { &hf_sample_content_filt,
      { "Number of Content Filter Entries",
        "opendds.sample.content_filter_entries",
        FT_UINT32, BASE_HEX, NULL_HFILL
      }
    }
  };

  static gint *ett[] = {
    &ett_trans_header,
    &ett_trans_flags,
    &ett_sample_header,
    &ett_sample_flags
  };

  proto_opendds = proto_register_protocol(
    "OpenDDS DCPS Protocol",  // name
    "OpenDDS",                // short_name
    "opendds");               // filter_name

  proto_register_field_array(proto_opendds, hf, array_length(hf));
  proto_register_subtree_array(ett, array_length(ett));
}

extern "C"
dissector_Export void
proto_reg_handoff_opendds()
{
  dcps_tcp_handle = create_dissector_handle(dissect_opendds, proto_opendds);
  //  dcps_giop_handle = create_dissector_handle(dissect_inforepo_giop, proto_info_repo);

  heur_dissector_add("tcp", dissect_opendds_heur, proto_opendds);
  heur_dissector_add("udp", dissect_opendds_heur, proto_opendds);
#if 0
  ACE_DEBUG ((LM_DEBUG, "calling register_giop_user\n"));
  register_giop_user_module(dissect_inforepo_giop, "InfoRepo", "OpenDDS/DCPS/DCPSInfo", proto_info_repo);
  register_giop_user(dissect_inforepo_giop_heur, "InfoRepo", proto_info_repo);
  ACE_DEBUG ((LM_DEBUG, "giop user registered\n"));
#endif

  dissector_add_handle("tcp.port", dcps_tcp_handle);  /* for "decode-as" */
  // dissector_add_handle("tcp.port", dcps_giop_handle);  /* for "decode-as" */
}
