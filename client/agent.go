package main

import (
	"log"
	"math/rand"

	"golang.org/x/net/websocket"
)

type Candidate struct {
	Member *int
	You    *int
	Say    string
}

type Ack struct {
	OK bool
}

type Select struct {
	Index int
}

func main() {
	conn, err := websocket.Dial("ws://localhost:8090/senda", "", "localhost:8090")
	if err != nil {
		log.Println(err)
		return
	}
	for {
		var cand Candidate
		err = websocket.JSON.Receive(conn, &cand)
		if err != nil {
			log.Println(err)
			return
		}
		log.Println(cand.Say)
		if cand.Member == nil {
			log.Println("ack")
			err = websocket.JSON.Send(conn, &Ack{OK: true})
			if err != nil {
				log.Println(err)
				return
			}
		} else {
			log.Println("select")
			for {
				i := rand.Intn(*cand.Member)
				if i == *cand.You {
					continue
				}
				websocket.JSON.Send(conn, &Select{Index: i})
				break
			}
		}
	}
}
