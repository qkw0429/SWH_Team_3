#pragma once

#include <stdexcept>
#include <string>
#include "Domain/DTO/DVMInfoDTO.h"

using namespace std;

namespace customException {

    class CustomException : public std::runtime_error {
    public:
        explicit CustomException(const std::string& message) 
            : std::runtime_error(message) {}
        
        virtual ~CustomException() = default;
    };

    class NotFoundException : public CustomException {
    public:
        explicit NotFoundException(const string& message) 
            : CustomException("Not Found: " + message) {}
    };

    class InvalidException : public CustomException {
    public:
        explicit InvalidException(const string& message) 
            : CustomException("Invalid: " + message) {}
    };

    class DuplicateException : public CustomException {
    public:
        explicit DuplicateException(const string& message) 
            : CustomException("Duplicate: " + message) {}
    };

    class NotEnoughStockException : public CustomException {
    public:
        explicit NotEnoughStockException(const string& message) 
            : CustomException(message) {}
    };

    class NotEnoughBalanceException : public CustomException {
    public:
        explicit NotEnoughBalanceException(const string& message) 
            : CustomException(message) {}
    };

    class FailedToPrePaymentException : public CustomException {
    public:
        explicit FailedToPrePaymentException(const string& message) 
            : CustomException(message) {}

    };

    class DVMInfoException : public CustomException {
        private:
            DVMInfoDTO nearestDVM_;        
    
        public:
            // 메시지를 CustomException에게 넘기고, DVMInfoDTO도 보관
            explicit DVMInfoException(const DVMInfoDTO& info) noexcept
                : CustomException("Nearest DVM information available"), nearestDVM_(info) {}

            const DVMInfoDTO& getNearestDVM() const noexcept {
                return nearestDVM_;
            }
    };

    class FileOpenException : public CustomException {
    public:
        explicit FileOpenException(const string& message) 
            : CustomException("File Open Error: " + message) {}
    };
}