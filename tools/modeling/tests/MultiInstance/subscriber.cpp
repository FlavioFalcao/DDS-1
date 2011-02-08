
#include <ace/Log_Msg.h>
#include <ace/ARGV.h>

#include <dds/DCPS/WaitSet.h>

#ifdef ACE_AS_STATIC_LIBS
#include <dds/DCPS/transport/simpleTCP/SimpleTcp.h>
#endif

#include "model/MultiInstanceTraits.h"
#include <model/NullReaderListener.h>
#include <model/Sync.h>

class ReaderListener : public OpenDDS::Model::NullReaderListener {
  virtual void on_data_available(
    DDS::DataReader_ptr reader)
  ACE_THROW_SPEC((CORBA::SystemException));
};

// START OF EXISTING MESSENGER EXAMPLE LISTENER CODE

void
ReaderListener::on_data_available(DDS::DataReader_ptr reader)
ACE_THROW_SPEC((CORBA::SystemException))
{
  data1::MessageDataReader_var reader_i =
    data1::MessageDataReader::_narrow(reader);

  if (CORBA::is_nil(reader_i.in())) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: on_data_available() -")
               ACE_TEXT(" _narrow failed!\n")));
    ACE_OS::exit(-1);
  }

  data1::Message message;
  DDS::SampleInfo info;

  DDS::ReturnCode_t error = reader_i->take_next_sample(message, info);

  if (error == DDS::RETCODE_OK) {
    std::cout << "SampleInfo.sample_rank = " << info.sample_rank << std::endl;
    std::cout << "SampleInfo.instance_state = " << info.instance_state << std::endl;

    if (info.valid_data) {
      std::cout << "Message: subject    = " << message.subject.in() << std::endl
                << "         subject_id = " << message.subject_id   << std::endl
                << "         from       = " << message.from.in()    << std::endl
                << "         count      = " << message.count        << std::endl
                << "         text       = " << message.text.in()    << std::endl;

    }

  } else {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: on_data_available() -")
               ACE_TEXT(" take_next_sample failed!\n")));
  }
}

// END OF EXISTING MESSENGER EXAMPLE LISTENER CODE

template<class ModelType>
int run_instance(ModelType& model) {
    using OpenDDS::Model::MultiInstance::Elements;

    DDS::DataReader_var reader = model.reader( Elements::DataReaders::reader);

    DDS::DataReaderListener_var listener(new ReaderListener);
    reader->set_listener( listener.in(), OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    // START OF EXISTING MESSENGER EXAMPLE CODE

    data1::MessageDataReader_var reader_i =
      data1::MessageDataReader::_narrow(reader);

    if (CORBA::is_nil(reader_i.in())) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("ERROR: %N:%l: run_instance() -")
                        ACE_TEXT(" _narrow failed!\n")),
                       -1);
    }

    OpenDDS::Model::ReaderSync rs(reader);

    // END OF EXISTING MESSENGER EXAMPLE CODE

  return 0;
}

int ACE_TMAIN(int argc, ACE_TCHAR** argv)
{
  int result;
  ACE_ARGV argv_copy(argc, argv);
  ACE_ARGV argv_copy2(argc, argv);
  try {
    OpenDDS::Model::Application application(argc, argv);
    {
      MultiInstance::PrimaryMultiInstanceType primary_model(application,
                                                            argc, 
                                                            argv_copy.argv());
      std::cout << "Running primary subscriber instance" << std::endl;
      result = run_instance(primary_model);
      std::cout << "Primary subscriber instance complete" << std::endl;
    }
    if (!result) {
      int argc_copy = argv_copy.argc();
      MultiInstance::SecondaryMultiInstanceType secondary_model(application, 
                                                                argc_copy, 
                                                                argv_copy2.argv());
      std::cout << "Running secondary subscriber instance" << std::endl;
      result = run_instance(secondary_model);
      std::cout << "Secondary subscriber instance complete" << std::endl;
    }
  } catch (const CORBA::Exception& e) {
    e._tao_print_exception("Exception caught in main():");
    return -1;

  } catch( const std::exception& ex) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: %N:%l: main() -")
                      ACE_TEXT(" Exception caught: %C\n"),
                      ex.what()),
                     -1);
  }

  return result;
}
