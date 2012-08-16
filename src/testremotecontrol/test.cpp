#include <string>
#include <map>
#include <unistd.h>

#include "RemoteControl.h"

using namespace std;

class TestControllable : public RemoteControllable
{
    public:
        TestControllable(string name) : RemoteControllable(name)
        {
            RC_ADD_PARAMETER(foo, "That's the foo");
            RC_ADD_PARAMETER(bar, "That's the bar");
            RC_ADD_PARAMETER(baz, "That's the baz");
        }

        void set_parameter(string parameter, string value) {
            stringstream ss(value);
            ss.exceptions ( stringstream::failbit | stringstream::badbit );

            if (parameter == "foo") {
                ss >> foo_;
            }
            else if (parameter == "bar") {
                bar_ = value;
            }
            else if (parameter == "baz") {
                ss >> baz_;
            }
            else {
                stringstream ss;
                ss << "Parameter '" << parameter << "' is not exported by controllable " << get_rc_name();
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
        double baz_;

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

