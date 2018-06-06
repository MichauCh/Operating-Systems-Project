//
// Created by michau on 01.06.18.
//

#ifndef KINO_LUDZIE_H

#define KINO_LUDZIE_H


class Client{
public:

    int trans_time;
    int hall;

    Client(int transaction_time, int hall) {
        this -> trans_time = transaction_time;
        this -> hall = hall;
    }

    Client() {}

};

/*class Cleaner{
public:

    int hall;

}; */

#endif //KINO_LUDZIE_H
