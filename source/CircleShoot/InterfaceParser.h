#include <SexyAppFramework/XMLParser.h>

namespace Sexy
{
	class InterfaceParser {
	private:
		bool mHasFailed;
	public:
		XMLParser* mXMLParser;
		int i;

		InterfaceParser();
		bool ParseInterfaceFile(const std::string& theFilename);
	};
}