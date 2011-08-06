// -*- C++ -*-
//
// $Id$
#ifndef OPTIONS_H
#define OPTIONS_H

// Needed here to avoid the pragma below when necessary.
#include /**/ "ace/pre.h"
#include /**/ "ace/config-all.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

#include "dds/DCPS/DataCollector_T.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace Test {

  class PublicationProfile;

/**
 * @class Options
 *
 * @brief manage execution options
 *
 * This class extracts option information from the command line and makes
 * it available to the process.
 *
 * Options extracted by this class are:
 *
 *   -v
 *      Be verbose when executing.
 *
 *   -t [ tcp | udp | mc ]
 *      Establish the transport implementation for data.  The values are:
 *        tcp - use the Tcp transport
 *        udp - use the udp transport
 *        mc  - use the multicast transport
 *
 *   -c <seconds>
 *      Run test for <seconds> seconds.  If not specified, the test will
 *      not terminate until killed by an external signal.
 *
 *   -f <file>
 *      Extract detailed scenario parameters from <file>.  The format of
 *      the file is a set of KeyValue pairs organized into sections.
 *      There is a common section that can be used to define options
 *      common to the entire process and a separate subsection for each
 *      publication to be instantiated for the scenario.
 *
 *      The key/value pairs which can be specified outside of all
 *      sections include:
 *
 *        Transport = <type>
 *
 *      Where <type> can be one of: tcp, udp, mc or rmc.
 *
 *        TestDuration = <seconds>
 *
 *      Where the test will continue for <seconds> and then terminate.
 *      If not specified, the test will continue until an external signal
 *      terminates it.
 *
 *      Publication subsections are named as:
 *
 *        [publication/<name>]
 *
 *      Where <name> is a unique identifier for that publication.  The
 *      key/value pairs that can be specified within a publication
 *      subsection include:
 *
 *        Priority = <priority>
 *
 *          Where <priority> is an integer numeric value corresponding
 *          to a Quality of Service policy value for TRANSPORT_PRIORITY.
 *
 *        MessageRate = <messages per second>
 *
 *          Where <messages per second> is an integer numeric value
 *          specifying the average rate at which messages are to be
 *          generated by this publication.
 *
 *        MessageSize      = <bytes per message>
 *        MessageMax       = <bytes per message>
 *        MessageMin       = <bytes per message>
 *        MessageDeviation = <bytes per message>
 *
 *          Where <bytes per message> are integer numeric values
 *          specifying the characteristics of the message sizes generated
 *          and sent by this publication.  This includes the average
 *          (MessageSize), maximum (MessageMax), minimum (MessageMin) and
 *          standard deviation (MessageDeviation) values used to
 *          generate a Gaussian distribution of message sizes truncated
 *          at the maximum and minimum specified values.
 */
class Options  {
  public:
    /// Types of transport implementations supported.
    enum TransportType {
      TRANSPORT_NONE,     // Unsupported (NONE is a macro on VxWorks)
      TCP,      // Tcp
      UDP,      // udp
      MC        // multicast
    };

    /// Container type for publication profiles.
    typedef std::vector< PublicationProfile*> ProfileContainer;

    /// Default constructor.
    Options(int argc, ACE_TCHAR** argv, char** envp = 0);

    /// Virtual destructor.
    virtual ~Options();

    /// Test verbosity.
    protected: bool& verbose();
    public:    bool  verbose() const;

    /// Test domain.
    protected: unsigned long& domain();
    public:    unsigned long  domain() const;

    /// Test duration.
    protected: long& duration();
    public:    long  duration() const;

    /// Transport Type value.
    protected: TransportType& transportType();
    public:    TransportType  transportType() const;

    /// Transport Key value, translated from the type.
    protected: std::string& transportKey();
    public:    std::string  transportKey() const;

    /// Test topic name.
    protected: std::string& topicName();
    public:    std::string topicName() const;

    /// Raw data output file.
    protected: std::string& rawOutputFilename();
    public:    std::string  rawOutputFilename() const;

    /// Raw latency data buffer size.
    protected: unsigned int& raw_buffer_size();
    public:    unsigned int  raw_buffer_size() const;

    /// Raw latency data buffer type.
    protected: OpenDDS::DCPS::DataCollector< double>::OnFull& raw_buffer_type();
    public:    OpenDDS::DCPS::DataCollector< double>::OnFull  raw_buffer_type() const;

    /// Publication profile container.
    const ProfileContainer& profiles() const;

  private:
    /// Configure scenario information from a file.
    void configureScenarios(const ACE_TCHAR* filename);

    /// Test verbosity.
    bool verbose_;

    /// Test domain.
    unsigned long domain_;

    /// Process identifier.
    long id_;

    /// Test duration in seconds: -1 indicates no timed termination.
    long duration_;

    /// Transport Type value.
    TransportType transportType_;

    /// Transport Key value.
    std::string transportKey_;

    /// Topic name for test.
    std::string topicName_;

    /// Raw data output file.
    std::string rawOutputFilename_;

    /// Raw latency data buffer size.
    unsigned int raw_buffer_size_;

    /// Raw latency data buffer type.
    OpenDDS::DCPS::DataCollector< double>::OnFull raw_buffer_type_;

    /// PublicationProfile container.
    ProfileContainer publicationProfiles_;
};

} // End of namespace Test

std::ostream& operator<<(std::ostream& str, Test::Options::TransportType value);

#if defined (__ACE_INLINE__)
# include "Options.inl"
#endif  /* __ACE_INLINE__ */

#endif // OPTIONS_H

