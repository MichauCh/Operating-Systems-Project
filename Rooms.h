//
// Created by michau on 01.06.18.
//

#ifndef KINO_POMIESZCZENIA_H
#define KINO_POMIESZCZENIA_H

class Cinema_hall {
public:
    int seats = 128;
    int population = 0;
    int nr;
    bool movie = false;
    bool end_show = false;

    Cinema_hall(int nr) {
        this -> nr = nr;
    }
};

class Toaleta {
public:
    int cabins = 10;
    int population = 0;
    int nr;     //do identyfikacji pomieszczenia

};
#endif //KINO_POMIESZCZENIA_H
