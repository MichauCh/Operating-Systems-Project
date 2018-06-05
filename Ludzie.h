//
// Created by michau on 01.06.18.
//

#ifndef KINO_LUDZIE_H

#define KINO_LUDZIE_H


class Client{
public:

    int czas_transakcji;
    int hall;

    Client(int transaction_time, int hall) {
        this -> czas_transakcji = transaction_time;
        this -> hall = hall;
    }

    Client() {}

};

/*class Cleaner{
public:

    int hall;

}; */

#endif //KINO_LUDZIE_H
