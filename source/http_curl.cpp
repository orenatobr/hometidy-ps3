#include "http_curl.h"

#include <curl/curl.h>

namespace http {
namespace {

class CurlClient : public Client {
public:
    CurlClient(std::string caFile, std::string userAgent) : ca_(std::move(caFile)), agent_(std::move(userAgent)) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        multi_ = curl_multi_init();
    }
    ~CurlClient() override {
        drop();
        if (multi_) curl_multi_cleanup(multi_);
        curl_global_cleanup();
    }

    void start(const std::string& url, std::size_t maxBytes) override {
        drop();
        body_.clear();
        max_ = maxBytes;
        prog_ = {};
        status_ = 0;
        error_.clear();
        tooLarge_ = false;
        easy_ = curl_easy_init();
        if (!easy_) return fail("could not start the transfer");
        curl_easy_setopt(easy_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(easy_, CURLOPT_USERAGENT, agent_.c_str());
        curl_easy_setopt(easy_, CURLOPT_WRITEFUNCTION, &CurlClient::onWrite);
        curl_easy_setopt(easy_, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(easy_, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(easy_, CURLOPT_XFERINFOFUNCTION, &CurlClient::onProgress);
        curl_easy_setopt(easy_, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(easy_, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(easy_, CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(easy_, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);  // never plain http, also after a redirect
        curl_easy_setopt(easy_, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
        curl_easy_setopt(easy_, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(easy_, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(easy_, CURLOPT_CAINFO, ca_.c_str());
        curl_easy_setopt(easy_, CURLOPT_ACCEPT_ENCODING, "");  // gzip: the cover lists shrink by about 80%
        curl_easy_setopt(easy_, CURLOPT_CONNECTTIMEOUT, 20L);
        curl_easy_setopt(easy_, CURLOPT_LOW_SPEED_LIMIT, 1L);  // give up when nothing arrives for 30 s
        curl_easy_setopt(easy_, CURLOPT_LOW_SPEED_TIME, 30L);
        curl_easy_setopt(easy_, CURLOPT_NOSIGNAL, 1L);
        headers_ = curl_slist_append(nullptr, "Accept: application/vnd.github+json");
        curl_easy_setopt(easy_, CURLOPT_HTTPHEADER, headers_);
        if (curl_multi_add_handle(multi_, easy_) != CURLM_OK) return fail("could not start the transfer");
        state_ = State::Running;
    }

    State step() override {
        if (state_ != State::Running) return state_;
        int running = 0;
        curl_multi_perform(multi_, &running);
        int left = 0;
        while (CURLMsg* m = curl_multi_info_read(multi_, &left)) {
            if (m->msg != CURLMSG_DONE) continue;
            if (m->data.result == CURLE_OK) {
                long code = 0;
                curl_easy_getinfo(easy_, CURLINFO_RESPONSE_CODE, &code);
                status_ = static_cast<int>(code);
                state_ = State::Done;
            } else {
                fail(tooLarge_ ? "the answer is larger than expected" : curl_easy_strerror(m->data.result));
            }
        }
        return state_;
    }

    Progress progress() const override { return prog_; }
    int status() const override { return status_; }
    std::string error() const override { return error_; }
    std::vector<std::uint8_t> take() override { return std::move(body_); }
    void cancel() override {
        drop();
        state_ = State::Idle;
    }

private:
    static size_t onWrite(char* data, size_t size, size_t n, void* self) {
        auto* c = static_cast<CurlClient*>(self);
        const size_t bytes = size * n;
        if (c->body_.size() + bytes > c->max_) {
            c->tooLarge_ = true;
            return 0;  // aborts the transfer
        }
        c->body_.insert(c->body_.end(), reinterpret_cast<std::uint8_t*>(data), reinterpret_cast<std::uint8_t*>(data) + bytes);
        return bytes;
    }
    static int onProgress(void* self, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
        auto* c = static_cast<CurlClient*>(self);
        c->prog_.done = dlnow;
        c->prog_.total = dltotal > 0 ? dltotal : -1;
        return 0;
    }
    void fail(const std::string& why) {
        error_ = why;
        state_ = State::Failed;
        drop();
    }
    void drop() {
        if (easy_) {
            curl_multi_remove_handle(multi_, easy_);
            curl_easy_cleanup(easy_);
            easy_ = nullptr;
        }
        if (headers_) {
            curl_slist_free_all(headers_);
            headers_ = nullptr;
        }
    }

    std::string ca_, agent_;
    CURLM* multi_ = nullptr;
    CURL* easy_ = nullptr;
    curl_slist* headers_ = nullptr;
    State state_ = State::Idle;
    std::vector<std::uint8_t> body_;
    std::size_t max_ = 0;
    bool tooLarge_ = false;
    Progress prog_;
    int status_ = 0;
    std::string error_;
};

}  // namespace

std::unique_ptr<Client> makeCurlClient(const std::string& caFile, const std::string& userAgent) {
    return std::make_unique<CurlClient>(caFile, userAgent);
}

}  // namespace http
