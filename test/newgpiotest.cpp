#include <cstdlib>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fstream>
#include "GPIOPin.h"
#include "GPIOAccess.h"

using namespace std;

#define numPins 27

void usage(char * nm) {
    printf("Usage\n");
    printf("Commands - one of:\n");
    printf("\t- %s <op> <pin> <val>\n",nm);
    printf("\t- %s pwm <pin> <freq> <duty>\n",nm);
    printf("\t\tStarts PWM output on pin\n");
    printf("\t- %s pwmstop <pin>\n",nm);
    printf("\t\tStops PWM output on pin\n");
    printf("\t- %s irq <pin> <irqtype> <irqcmd> <debounce>\n",nm);
    printf("\t\tEnables IRQ handling on pin\n");
    printf("\t- %s irqstop <pin>\n",nm);
    printf("\t\tTerminates IRQ handling on pin\n");
    printf("Where:\n");
    printf("\t<op> is one of:\n");
    printf("\t\tinfo - to display info on pin(s)\n");
    printf("\t\tset - to set pin(s) value\n");
    printf("\t\tget - to get and return pin value\n");
    printf("\t\tsetd - to set pin(s) direction\n");
    printf("\t\tgetd - to get and return pin direction\n");
    printf("\t\thelp - to display usage\n");
    printf("\t<pin> is one of\n\t\t0, 1, 6, 7, 8, 12, 13, 14, 15, 16, 17, 18, 19, 23, 26, all\n");
    printf("\t\tA <pin> of all can only be used for an <op> of:\n");
    printf("\t\t\tinfo, set, setd\n");
    printf("\t<val> is only required for set and setd:\n");
    printf("\t\tfor set, <val> is 0 or 1\n");
    printf("\t\tfor setd, <val> is in or out\n");
    printf("\t<freq> is PWM frequency in Hz > 0\n");
    printf("\t<duty> is PWM duty cycle %% in range 0 to 100\n");
    printf("\t<irqtype> is the type for IRQ and is one of:\n");
    printf("\t\tfalling, rising, both\n");
    printf("\t<irqcmd> is the shell command to be executed when the IRQ occurs\n");
    printf("\t\tMust be enclosed in \" characters if it contains\n");
    printf("\t\tspaces or other special characters\n");
    printf("\t\tIf it starts with the string [debug],\n");
    printf("\t\tdebug output is displayed first\n");
    printf("\t<debounce> is optional debounce time for IRQ in milliseconds\n");
    printf("\t\tDefaults to 0 if not supplied\n");
}

#define opinfo 0
#define opset 1
#define opget 2
#define opsetd 3
#define opgetd 4
#define oppwm 5
#define oppwmstop 6
#define opirq 7
#define opirqstop 8
int operation;
int pinnum;
int value;
int freq;
int duty;
GPIO_Direction direction;
GPIO_Irq_Type irqType;
char irqcmd[200];
long int debounce;

class GPIO_Irq_Command_Handler_Object : public GPIO_Irq_Handler_Object {
public:
    GPIO_Irq_Command_Handler_Object(char * com) {
        strcpy(this->cmd, com);
    }

    void handleIrq(int pinNum, GPIO_Irq_Type type) {
        if (strstr(cmd, "[debug]") == cmd) {
            char dbgcmd[300];
            sprintf(dbgcmd, "echo 'GPIO Irq Debug: Pin=%d Type=", pinNum);
            if (type == GPIO_IRQ_RISING) {
                strcat(dbgcmd, "Rising' && ");
            } else {
                strcat(dbgcmd, "Falling' && ");
            }
            strcat(dbgcmd, cmd + 7);
            system(dbgcmd);
        } else {
            system(cmd);
        }
    }
    
private:
    char cmd[200];
};

bool processArgs(int argc, char** argv) {
    operation = -1;
    pinnum = -1;
    value = -1;
    if (argc > 1) {
        if (strcmp(argv[1], "info") == 0) {
            operation = opinfo;
        } else if (strcmp(argv[1], "set") == 0) {
            operation = opset;
        } else if (strcmp(argv[1], "get") == 0) {
            operation = opget;
        } else if (strcmp(argv[1], "setd") == 0) {
            operation = opsetd;
        } else if (strcmp(argv[1], "getd") == 0) {
            operation = opgetd;
        } else if (strcmp(argv[1], "pwm") == 0) {
            operation = oppwm;
        } else if (strcmp(argv[1], "pwmstop") == 0) {
            operation = oppwmstop;
        } else if (strcmp(argv[1], "irq") == 0) {
            operation = opirq;
        } else if (strcmp(argv[1], "irqstop") == 0) {
            operation = opirqstop;
        } else if (strcmp(argv[1], "help") == 0) {
            return false;
        } else {
            printf("**ERROR** Invalid <op> : %s\n", argv[1]);
            return false;
        }
        
        if (argc > 2) {
            if (strcmp(argv[2], "all") == 0) {
                if ((operation != opset) && 
                        (operation != opsetd) &&
                        (operation != opinfo)) {
                    printf("**ERROR** Invalid <pin> for operation: %s\n", argv[2]);
                    return false;
                }
                pinnum = -1;
            } else if (strcmp(argv[2], "0") == 0) {
                pinnum = 0;
                if (!GPIOAccess::isPinUsable(pinnum)) {
                    printf("**ERROR** Invalid <pin> : %s\n", argv[2]);
                    return false;
                }
            } else {
                pinnum = strtol(argv[2], NULL, 10);
                if (pinnum != 0) {
                    if (!GPIOAccess::isPinUsable(pinnum)) {
                        printf("**ERROR** Invalid <pin> : %s\n", argv[2]);
                        return false;
                    }
                } else {
                    printf("**ERROR** Invalid <pin> : %s\n", argv[2]);
                    return false;
                }
            }
            
            if (operation == opset) {
                if (argc > 3) {
                    if (strcmp(argv[3], "0") == 0) {
                        value = 0;
                    } else if (strcmp(argv[3], "1") == 0 ) {
                        value = 1;
                    } else{
                        printf("**ERROR** Invalid <val> for set: %s\n", argv[3]);
                        return false;
                    }
                } else {
                    printf("**ERROR** No <val> specified for set\n");
                    return false;
                }
            } else if (operation == opsetd) {
                if (argc > 3) {
                    if (strcmp(argv[3], "in") == 0) {
                        direction = GPIO_INPUT;
                    } else if (strcmp(argv[3], "out") == 0 ) {
                        direction = GPIO_OUTPUT;
                    } else{
                        printf("**ERROR** Invalid <val> for setd: %s\n", argv[3]);
                        return false;
                    }
                } else {
                    printf("**ERROR** No <val> specified for setd\n");
                    return false;
                }
            } else if (operation == opirq) {
                if (argc > 3) {
                    if (strcmp(argv[3], "falling") == 0 ) {
                        irqType = GPIO_IRQ_FALLING;
                    } else if (strcmp(argv[3], "rising") == 0 ) {
                        irqType = GPIO_IRQ_RISING;
                    } else if (strcmp(argv[3], "both") == 0 ) {
                        irqType = GPIO_IRQ_BOTH;
                    } else{
                        printf("**ERROR** Invalid <irqtype> for irq: %s\n", argv[3]);
                        return false;
                    }
                    if (argc > 4) {
                        strcpy(irqcmd, argv[4]);
                        if (argc > 5) {
                            if (strcmp(argv[5], "0") == 0) {
                                debounce = 0;
                            } else {
                                debounce = strtol(argv[5], NULL, 10);
                                if (debounce == 0) {
                                    printf("**ERROR** Invalid <debounce> for irq : %s\n", argv[5]);
                                    return false;
                                }
                            }                            
                        } else {
                            debounce = 0;
                        }
                    } else {
                        printf("**ERROR** No <irqcmd> specified for irq\n");
                        return false;
                    }
                } else {
                    printf("**ERROR** No <irqtype> specified for irq\n");
                    return false;
                }
            } else if (operation == oppwm) {
                if (argc > 3) {
                    freq = strtol(argv[3], NULL, 10);
                    if (freq > 0) {
                        if (argc > 4) {
                            duty = strtol(argv[4], NULL, 10);
                            if ((duty < 0) || (duty > 100)) {
                                printf("**ERROR** Invalid <duty> for pwm : %s\n", argv[4]);
                                return false;
                            }
                        } else {
                            printf("**ERROR** No <duty> specified for pwm\n");
                            return false;
                        }
                    } else {
                        printf("**ERROR** Invalid <freq> for pwm : %s\n", argv[3]);
                        return false;
                    }
                } else {
                    printf("**ERROR** No <freq> specified for pwm\n");
                    return false;
                }
            }
        } else {
            printf("**ERROR** No <pin> specified\n");
            return false;
        }
    } else {
        printf("**ERROR** No <op> specified\n");
        return false;
    }
    
    return true;
}

#define PID_FILE_PWM	"/tmp/pin%d_pwm_pid"
#define PID_FILE_IRQ	"/tmp/pin%d_irq_pid"

void stopIRQ(int pinnum) {
    char 	pathname[255];
    char	line[255];
    char	cmd[255];

    int 	pid;
    std::ifstream myfile;

    // determine the file name and open the file
    snprintf(pathname, sizeof(pathname), PID_FILE_IRQ, pinnum);
    myfile.open (pathname);

    // read the file
    if ( myfile.is_open() ) {
        // file exists, check for pid
        myfile.getline(line, 255);
        pid = atoi(line);

        // kill the process
        if (pid > 0)
        {
                sprintf(cmd, "kill %d >& /dev/null", pid);
                system(cmd);
        }

        sprintf(cmd, "rm %s >& /dev/null", pathname);
        system(cmd);

        myfile.close();
    }
}

void stopPWM(int pinnum) {
    char 	pathname[255];
    char	line[255];
    char	cmd[255];

    int 	pid;
    std::ifstream myfile;

    // determine the file name and open the file
    snprintf(pathname, sizeof(pathname), PID_FILE_PWM, pinnum);
    myfile.open (pathname);

    // read the file
    if ( myfile.is_open() ) {
        // file exists, check for pid
        myfile.getline(line, 255);
        pid = atoi(line);

        // kill the process
        if (pid > 0)
        {
                sprintf(cmd, "kill %d >& /dev/null", pid);
                system(cmd);
        }

        sprintf(cmd, "rm %s >& /dev/null", pathname);
        system(cmd);
            
        myfile.close();
    }
}

bool getPWMInf(int pinnum, char * inf) {
    char 	pathname[255];
    char	line[255];
    char	cmd[255];

    int 	pid;
    std::ifstream myfile;

    // determine the file name and open the file
    snprintf(pathname, sizeof(pathname), PID_FILE_PWM, pinnum);
    myfile.open (pathname);

    // read the file
    if ( myfile.is_open() ) {
        bool ret = false;
        // file exists, check for pid
        myfile.getline(line, 255);
        pid = atoi(line);        
        if (kill(pid, 0) == 0)
        {
            // Process is running - get the info
            char infostr[128];
            sprintf (infostr, "\tProcess Id:%d\n\t", pid);
            myfile.getline(line, 255);
            strcat(infostr, line);
            strcpy(inf, infostr);
            ret = true;
        }        

        myfile.close();
        
        return ret;
    }
    
    return false;
}


bool getIRQInf(int pinnum, char * inf) {
    char 	pathname[255];
    char	line[255];
    char	cmd[255];

    int 	pid;
    std::ifstream myfile;

    // determine the file name and open the file
    snprintf(pathname, sizeof(pathname), PID_FILE_IRQ, pinnum);
    myfile.open (pathname);

    // read the file
    if ( myfile.is_open() ) {
        bool ret = false;
        // file exists, check for pid
        myfile.getline(line, 255);
        pid = atoi(line);        
        if (kill(pid, 0) == 0)
        {
            // Process is running - get the info
            char infostr[128];
            sprintf (infostr, "\tProcess Id:%d\n\t", pid);
            myfile.getline(line, 255);
            strcat(infostr, line);
            strcpy(inf, infostr);
            ret = true;
        }        

        myfile.close();
        
        return ret;
    }
    
    return false;
}

void noteChildPIDPWM(int pinnum, int pid, int freq, int duty) {
    char 	pathname[255];
    std::ofstream myfile;

    // determine the file name and open the file
    snprintf(pathname, sizeof(pathname), PID_FILE_PWM, pinnum);
    myfile.open (pathname);

    // write the pid to the file
    myfile << pid;
    myfile << "\n";
    
    char inf[100];
    sprintf(inf, "Frequency:%d, Duty:%d\n", freq, duty);
    myfile << inf;

    myfile.close();
}

void noteChildPIDIRQ(int pinnum, int pid, GPIO_Irq_Type irqType, char * irqcmd, int debounce) {
    char 	pathname[255];
    std::ofstream myfile;

    // determine the file name and open the file
    snprintf(pathname, sizeof(pathname), PID_FILE_IRQ, pinnum);
    myfile.open (pathname);

    // write the pid to the file
    myfile << pid;
    myfile << "\n";
    
    char inf[100];
    char irqTypeStr[20];
    if (irqType == GPIO_IRQ_RISING) {
        strcpy(irqTypeStr, "Rising");
    } else if (irqType == GPIO_IRQ_FALLING) {
        strcpy(irqTypeStr, "Falling");
    } else {
        strcpy(irqTypeStr, "Both");
    }
    sprintf(inf, "Type:%s, Cmd:'%s', Debounce:%d\n", irqTypeStr, irqcmd, debounce);
    myfile << inf;

    myfile.close();
}

void startPWM(int pinnum, int freq, int duty) {
    stopPWM(pinnum);
    stopIRQ(pinnum);

    // Continuous PWM output requires a separate process
    pid_t pid = fork();

    if (pid == 0) {
        // child process, run the pwm
        GPIOPin *pin = new GPIOPin(pinnum);
        pin->setDirection(GPIO_OUTPUT);
        pin->setPWM(freq, duty);
        GPIO_Result r = pin->getLastResult();
        if (r != GPIO_OK) {
            printf("**ERROR starting PWM on pin:%d to freq:%d, duty:%d err=%d\n", pinnum, freq, duty, r);
        }
        // Ensure child stays alive since PWM is running
        while (pin->isPWMRunning()) {
            usleep(10000000L);
        }
    }
    else {
        // parent process
        noteChildPIDPWM(pinnum, pid, freq, duty);
    }
}

void startIRQ(int pinnum, GPIO_Irq_Type irqType, char * irqcmd, int debounce) {
    stopPWM(pinnum);
    stopIRQ(pinnum);

    // IRQ handling requires a separate process
    pid_t pid = fork();

    if (pid == 0) {
        // child process, run the irq handler

        GPIO_Irq_Command_Handler_Object * irqHandlerObj = new GPIO_Irq_Command_Handler_Object(irqcmd);
        GPIOPin *pin = new GPIOPin(pinnum);
        pin->setDirection(GPIO_INPUT);
        pin->setIrq(irqType, irqHandlerObj, debounce);
        GPIO_Result r = pin->getLastResult();
        if (r != GPIO_OK) {
            printf("**ERROR starting IRQ on pin:%d with command:%s err=%d\n", pinnum, irqcmd, r);
        }
        // Ensure child stays alive since IRQ is running
        while (true) {
            usleep(10000000L);
        }
    }
    else {
        // parent process
        noteChildPIDIRQ(pinnum, pid, irqType, irqcmd, debounce);
    }
}

void setPin(int pinnum, int val) {
    stopPWM(pinnum);
    stopIRQ(pinnum);
    GPIOPin *pin = new GPIOPin(pinnum);
    pin->setDirection(GPIO_OUTPUT);
    pin->set(val);
    GPIO_Result r = pin->getLastResult();
    delete pin;
    if (r != GPIO_OK) {
        printf("**ERROR setting pin:%d to value:%d, err=%d\n", pinnum, val, r);
    }
}

void setPinDir(int pinnum, GPIO_Direction dir) {
    stopPWM(pinnum);
    stopIRQ(pinnum);
    GPIOPin *pin = new GPIOPin(pinnum);
    pin->setDirection(dir);
    GPIO_Result r = pin->getLastResult();
    delete pin;
    if (r != GPIO_OK) {
        printf("**ERROR setting pin direction:%d to direction:%d, err=%d\n", pinnum, dir, r);
    }
}

GPIO_Result getPin(int pinnum, int &val) {
    stopPWM(pinnum);
    GPIOPin *pin = new GPIOPin(pinnum);
    pin->setDirection(GPIO_INPUT);
    val = pin->get();
    GPIO_Result r = pin->getLastResult();
    delete pin;
    if (r != GPIO_OK) {
        printf("**ERROR getting pin:%d value, err=%d\n", pinnum, r);
    }
    return r;
}

GPIO_Result getPinDir(int pinnum, GPIO_Direction &dir) {
    GPIOPin *pin = new GPIOPin(pinnum);
    dir = pin->getDirection();
    GPIO_Result r = pin->getLastResult();
    delete pin;
    if (r != GPIO_OK) {
        printf("**ERROR getting pin:%d direction, err=%d\n", pinnum, r);
    }
    return r;
}


void reportOn(int pinnum) {
    printf("Pin:%d, ", pinnum);
    GPIOPin *pin = new GPIOPin(pinnum);
    GPIO_Direction dir = pin->getDirection();
    printf("Direction:");
    if (dir == GPIO_OUTPUT) {
        printf("OUTPUT");
        char inf[100];
        if (getPWMInf(pinnum, inf)) {
            printf(", PWM\n%s", inf);
        }
    } else if (dir == GPIO_INPUT) {
        int val = pin->get();
        printf("INPUT, Value:%d", val);
        char inf[100];
        if (getIRQInf(pinnum, inf)) {
            printf(" IRQ\n:%s", inf);
        }
    }
    printf("\n");
    delete pin;
}

int main(int argc, char** argv) {
    if (!processArgs(argc, argv)) {
        usage(argv[0]);
        return -1;
    }
    
    if (operation == opinfo) {
        if (pinnum != -1) {
            reportOn(pinnum);
        } else {
            int i;
            for (i = 0; i < numPins; i++) {
                if (GPIOAccess::isPinUsable(i)) {
                    reportOn(i);
                }
            }
        }
        return 0;
    } else if (operation == opset) {
        if (pinnum != -1) {
            setPin(pinnum, value);
        } else {
            int i;
            for (i = 0; i < numPins; i++) {
                if (GPIOAccess::isPinUsable(i)) {
                    setPin(i, value);
                }
            }
        }
        return 0;
    } else if (operation == opget) {
        int val;
        GPIO_Result r = getPin(pinnum, val);
        if (r == GPIO_OK) {
            return val;
        } else {
            return r;
        }
    } else if (operation == opsetd) {
        if (pinnum != -1) {
            setPinDir(pinnum, direction);
        } else {
            int i;
            for (i = 0; i < numPins; i++) {
                if (GPIOAccess::isPinUsable(i)) {
                    setPinDir(i, direction);
                }
            }
        }
        return 0;
    } else if (operation == opgetd) {
        GPIO_Direction dir;
        GPIO_Result r = getPinDir(pinnum, dir);
        if (r == GPIO_OK) {
            return dir;
        } else {
            return r;
        }
    } else if (operation == oppwm) {
        startPWM(pinnum, freq, duty);
        return 0;
    } else if (operation == oppwmstop) {
        stopPWM(pinnum);
        return 0;
    } else if (operation == opirq) {
        startIRQ(pinnum, irqType, irqcmd, debounce);
        return 0;
    } else if (operation == opirqstop) {
        stopIRQ(pinnum);
        return 0;
    }

    return -1;
}

