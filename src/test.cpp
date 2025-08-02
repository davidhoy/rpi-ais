#include <iostream>
#include <memory>
#include <string>

class Resource {
public:
    Resource(const std::string& name) : name_(name) {
        std::cout << "Resource " << name_ << " acquired\n";
    }
    ~Resource() {
        std::cout << "Resource " << name_ << " destroyed\n";
    }

    void use() const {
        std::cout << "Using resource: " << name_ << "\n";
    }

private:
    std::string name_;
};

class ResourceManager {
public:
    ResourceManager(const std::string& name)
        : resource_(std::make_unique<Resource>(name)) {}

    // Copy constructor - deleted, since unique_ptr can’t be copied
    ResourceManager(const ResourceManager&) = delete;

    // Move constructor
    ResourceManager(ResourceManager&& other) noexcept
        : resource_(std::move(other.resource_)) {}

    // Move assignment
    ResourceManager& operator=(ResourceManager&& other) noexcept {
        if (this != &other) {
            resource_ = std::move(other.resource_);
        }
        return *this;
    }

    // Use the resource
    void use() const {
        if (resource_) {
            resource_->use();
        } else {
            std::cout << "No resource to use\n";
        }
    }

private:
    std::unique_ptr<Resource> resource_;
};

int main() {
    ResourceManager mgr1("Audio");
    mgr1.use();

    ResourceManager mgr2 = std::move(mgr1);
    mgr2.use();
    mgr1.use(); // should say "No resource to use"

    return 0;
}
