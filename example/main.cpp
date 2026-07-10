#include <xyo/client.hpp>
#include <iostream>

int main() {
    try {
        // Instantiate the Client with a dummy API key configuration
        xyo::Client client(xyo::ClientConfig{
            /*.api_key =*/ "RandomBase64EncodedStringApiKey"
        });

        std::cout << "Successfully imported and instantiated the XYO Client (C++)\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
