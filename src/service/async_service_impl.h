#ifndef AUTHSERVICE_ASYNC_SERVICE_IMPL_H
#define AUTHSERVICE_ASYNC_SERVICE_IMPL_H

#include "envoy/service/auth/v3/external_auth.grpc.pb.h"
#include "config/config.pb.h"
#include "src/filters/filter_chain.h"
#include <boost/asio.hpp>
#include <grpcpp/grpcpp.h>

using namespace envoy::service::auth::v3;

namespace authservice {
namespace service {

class AsyncAuthServiceImpl {
 public:
  explicit AsyncAuthServiceImpl(config::Config config);

  void Run();

 private:
  config::Config config_;

  std::vector<std::unique_ptr<filters::FilterChain>> chains_;

  envoy::service::auth::v3::Authorization::AsyncService service_;
  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
  std::unique_ptr<grpc::Server> server_;

  std::shared_ptr<boost::asio::io_context> io_context_;

  std::chrono::seconds interval_in_seconds_;
  boost::asio::steady_timer timer_;
  std::function<void(const boost::system::error_code &ec)> timer_handler_function_;

  void SchedulePeriodicCleanupTask();
};

::grpc::Status Check(
        const ::envoy::service::auth::v3::CheckRequest *request,
        ::envoy::service::auth::v3::CheckResponse *response,
        std::vector<std::unique_ptr<filters::FilterChain>> &chains,
        const google::protobuf::RepeatedPtrField<config::TriggerRule> &trigger_rules_config,
        boost::asio::io_context& ioc,
        boost::asio::yield_context yield);

class ServiceState {
 public:
  virtual ~ServiceState() = default;

  virtual void Proceed() = 0;
};


class ProcessingState;

class CompleteState : public ServiceState {
 public:
  explicit CompleteState(ProcessingState *processor) : processor_(processor) {
  }

  void Proceed() override;

 private:
  ProcessingState *processor_;
};

class ProcessingState : public ServiceState {
 public:
  ProcessingState(std::vector<std::unique_ptr<filters::FilterChain>> &chains,
                  const google::protobuf::RepeatedPtrField<config::TriggerRule> &trigger_rules_config,
                  Authorization::AsyncService &service,
                  grpc::ServerCompletionQueue &cq, boost::asio::io_context &io_context);

  void Proceed() override;

 private:
  // GRPC service/queue/context
  Authorization::AsyncService &service_;
  grpc::ServerCompletionQueue &cq_;
  grpc::ServerContext ctx_;

  // The GRPC request we've received
  CheckRequest request_;
  // Used to send the GRPC response
  grpc::ServerAsyncResponseWriter<CheckResponse> responder_;

  // Boost::ASIO I/O service
  boost::asio::io_context &io_context_;

  std::vector<std::unique_ptr<filters::FilterChain>> &chains_;
  const google::protobuf::RepeatedPtrField<config::TriggerRule> &trigger_rules_config_;
};

}
}

#endif //AUTHSERVICE_ASYNC_SERVICE_IMPL_H
