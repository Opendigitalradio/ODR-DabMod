#include <string>
#include <unistd.h>
#include "RemoteControl.h"

using namespace std;

class TestControllable : public RemoteControllable
{
    public:
        TestControllable(string name)
        {
            name_ = name;
            parameterlist_.push_back("foo");
            parameterlist_.push_back("bar");
            parameterlist_.push_back("baz");
        }

        std::string get_rc_name() { return name_; };

        list<string> get_supported_parameters() {
            return parameterlist_;
        }

        void set_parameter(string parameter, string value) {
            if (parameter == "foo") {
                stringstream ss(value);
                ss >> foo_;
            }
            else if (parameter == "bar") {
                bar_ = value;
            }
            else if (parameter == "baz") {
                stringstream ss(value);
                ss >> baz_;
            }
            else {
                stringstream ss;
                ss << "Parameter '" << parameter << "' is not exported by controllable " << get_rc_name();
                throw ParameterError(ss.str());
            }
        }

        void set_parameter(string parameter, double value) {
            if (parameter == "baz") {
                baz_ = value;
            }
            else {
                stringstream ss;
                ss << "Parameter '" << parameter << "' is not a double in controllable " << get_rc_name();
                throw ParameterError(ss.str());
            }
        }

        void set_parameter(string parameter, long value) {
            if (parameter == "foo") {
                foo_ = value;
            }
            else {
                stringstream ss;
                ss << "Parameter '" << parameter << "' is not a long in controllable " << get_rc_name();
                throw ParameterError(ss.str());
            }
        }

        string get_parameter(string parameter) {
            stringstream ss;
            if (parameter == "foo") {
                ss << foo_;
            }
            else if (parameter == "bar") {
                ss << bar_;
            }
            else if (parameter == "baz") {
                ss << baz_;
            }
            else {
                stringstream ss;
                ss << "Parameter '" << parameter << "' is not exported by controllable " << get_rc_name();
                throw ParameterError(ss.str());
            }
            return ss.str();
        }

    private:
        long foo_;
        std::string bar_;
        std::string name_;
        double baz_;
        std::list<std::string> parameterlist_;

};

int main()
{
    RemoteControllerTelnet rc (2121);
    TestControllable t("test1");
    TestControllable t2("test2");

    t.enrol_at(rc);
    t2.enrol_at(rc);

    rc.start();

    std::cerr << "Thread has been launched" << std::endl;

    sleep(100);

    std::cerr << "Stop" << std::endl;

    rc.stop();

    return 0;
}

