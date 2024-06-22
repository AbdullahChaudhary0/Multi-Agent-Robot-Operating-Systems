#include <iostream>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <pthread.h>
#include <cstdlib>
#include <ctime>
#include <sys/shm.h>
#include <semaphore.h>
#include <cmath>
#include <sys/wait.h>
using namespace std;

// Forward declaration
struct robot;

int exitWidth = 0;	//True exit
float totalWidth = 0;	//Robots will estimate

// Function to calculate distance
double calculateDistance(const robot &r1, const robot &r2);

// Message structure
struct Message {
    long messageType;      // Message type (can be used to distinguish messages)
    int robotNumber;       // Number of the robot
    float estimatedWidth;  // Estimated width information
};

// Robot data
struct robot {
    int x;
    int y;
    float exitWidthEstimation;

    robot()
    {
        x = 0;
        y = 0;
        exitWidthEstimation = 0;
    }
};

// Creating 50 robots
robot robots[50]; // Creating 50 robots.

// Function to calculate distance
double calculateDistance(const robot &r1, const robot &r2)
{
    return sqrt(pow(r2.x - r1.x, 2) + pow(r2.y - r1.y, 2));
}

double calculateDistanceFromTheExit(const robot &r1, int exitWidth)
{
    double robotPosition = sqrt(pow(r1.x, 2) + pow(r1.y, 2));
    double distanceFromExit = abs(exitWidth - robotPosition);
    int result = 0;
    if (distanceFromExit <= 5)
    {
        result = rand() % 10 + 95;
        return result;
    }
    if (distanceFromExit <= 10)
    {
        result = rand() % 24 + 88;
        return result;
    }
    if (distanceFromExit <= 15)
    {
        result = rand() % 30 + 80;
        return result;
    }
    if (distanceFromExit <= 25)
    {
        result = rand() % 60 + 70;
        return result;
    }
    if (distanceFromExit <= 35)
    {
        result = rand() % 80 + 60;
        return result;
    }
    result = rand() % 100 + 50;
    return result;
}

bool areNeighbors(const robot &robot1, const robot &robot2, double maxDistance)
{
    double distance = calculateDistance(robot1, robot2);
    return distance <= maxDistance;
}

sem_t totalWidthSemaphore;
void *calculateAverageWidthNeighbor(void *arg)
{
    int i = *((int *)arg);
    int count = 1;
    // Use semaphore while communication between robots
    sem_wait(&totalWidthSemaphore);
    for (int j = 0; j < 50; j++)
    {
        if (i != j && areNeighbors(robots[i], robots[j], 5))
        {
            count++;
            robots[i].exitWidthEstimation += robots[j].exitWidthEstimation;
        }
    }

    
    robots[i].exitWidthEstimation /= count;
    totalWidth += robots[i].exitWidthEstimation;
    sem_post(&totalWidthSemaphore);

    pthread_exit(NULL);
}

int main()
{
    int shmid;	//Shared memory segment
    // Program
    srand(time(0));
    int dimension = 100;
    int room[dimension][dimension]; // Creating a 100 by 100 room.
    exitWidth = rand() % 11 + 16;	//16 to 26
    int numRobots = 50;

    // Giving random placement to robots in the room.
    for (int i = 0; i < 50; i++)
    {
        robots[i].x = rand() % 99 + 1;
        robots[i].y = rand() % 99 + 1;
    }

    // Create or access the shared memory segment for messages
    key_t key = ftok("/tmp", 'D');
    int msgid = msgget(key, 0666 | IPC_CREAT);

    // Sending data to shared memory
    for (int i = 0; i < 50; ++i)
    {
    	pid_t childPid = fork();

        if (childPid == -1) {
            // Handle error
            cout << "Error in creating process" << endl;
            return 1;
        }
        
        if (childPid == 0) {
        double result = calculateDistanceFromTheExit(robots[i], exitWidth);
        float estimatedWidth = (result * exitWidth) / 100;
        robots[i].exitWidthEstimation = estimatedWidth;

        cout << "Robot " << i + 1 << " Estimated Width: " << robots[i].exitWidthEstimation << endl;
        cout << "Sending data to shared memory." << endl;
        Message msg;
        msg.messageType = i + 1; // Use i + 1 as the message type
        msg.robotNumber = i + 1;
        msg.estimatedWidth = robots[i].exitWidthEstimation;
        msgsnd(msgid, &msg, sizeof(msg.robotNumber) + sizeof(msg.estimatedWidth), 0);
        }
        else{
        wait(NULL);
        exit(0);
        }
    }

    // Close the shared memory segment after writing
    // shmctl(shmid, IPC_RMID, NULL);

    // Reopen the shared memory segment for writing
    // shmid = shmget(key, sizeof(Message), 0666 | IPC_CREAT);

    // Reading data from the shared memory
    float robotNum[50], width[50];
    for (int i = 0; i < 50; i++)
    {
        Message receiveMsg;
        msgrcv(msgid, &receiveMsg, sizeof(receiveMsg.robotNumber) + sizeof(receiveMsg.estimatedWidth), i + 1, 0);

        robotNum[i] = receiveMsg.robotNumber;
        width[i] = receiveMsg.estimatedWidth;
    }

    sem_init(&totalWidthSemaphore, 0, 1); // Initialize the semaphore

    pthread_t threads[50];
    //Neighbors sharing data with each other through threads
    for (int i = 0; i < 50; i++)
    {
        int *threadId = static_cast<int *>(malloc(sizeof(int)));
        *threadId = i;
        pthread_create(&threads[i], NULL, calculateAverageWidthNeighbor, (void *)threadId);
    }

    // Join threads
    for (int i = 0; i < 50; i++)
    {
        pthread_join(threads[i], NULL);
    }
    for (int i=0; i<50; i++){
            cout <<"Robot: " <<robotNum[i] << " " <<" Approximated value: " << robots[i].exitWidthEstimation <<" True Exit Width: "<<exitWidth<<" Difference: "<<robots[i].exitWidthEstimation - exitWidth <<endl;
    }

    totalWidth /= numRobots;
    cout << "Average exit width calculated by all the robots is: " << totalWidth << ".\nWhile the truth value is: " << exitWidth << endl;

    cout << endl;
    sem_destroy(&totalWidthSemaphore); // Destroy the semaphore
    return 0;
}
