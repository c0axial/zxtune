#include <types.h>
#include <tools.h>
#include <error_tools.h>
#include <logging.h>
#include <parameters.h>
#include <binary/format.h>
#include <io/provider.h>
#include <iostream>

int main(int argc, char* argv[])
{
  if (argc < 3)
  {
    std::cout << *argv << " <filename> <pattern>" << std::endl;
    return 0;
  }
  try
  {
    const Binary::Format::Ptr format = Binary::Format::Create(argv[2]);
    const std::string filename = argv[1];
    const Parameters::Accessor::Ptr params = Parameters::Container::Create();
    Binary::Container::Ptr data;
    ThrowIfError(ZXTune::IO::OpenData(filename, *params, Log::ProgressCallback::Stub(), data));

    if (format->Match(data->Data(), data->Size()))
    {
      std::cout << "Matched" << std::endl;
    }
    else
    {
      const std::size_t offset = format->Search(data->Data(), data->Size());
      if (offset != data->Size())
      {
        std::cout << "Matched at offset " << offset << std::endl;
      }
      else
      {
        std::cout << "Not found" << std::endl;
      }
    }
  }
  catch (const std::exception& e)
  {
    std::cout << e.what() << std::endl;
  }
  catch (const Error& e)
  {
    std::cout << Error::ToString(e) << std::endl;
  }
}