///////////////////////////////////////////////////////////////////////////////////
//   Krampus Hack 2019
//      To: GullRaDriel
//      From: relpatseht
//
//    Wishlist:
//        I want a game that my 3 yo toddler can understand and 
//        play with me / along with me.
//        Anything is allowed, regarding it's targeting a lil' 3 
//        yo child, who will play together/under supervision of 
//        his parents, etc.
///////////////////////////////////////////////////////////////////////////////////

   _     U _____ u  _____ _   ____           ____     _   _             _      ____           _           ____     _   _     U  ___ u               __  __      _      _   _     _    
  |"|    \| ___"|/ |_ " _|"| / __"| u     U | __")uU |"|u| |   ___     |"|    |  _"\      U  /"\  u      / __"| u | \ |"|     \/"_ \/__        __ U|' \/ '|uU  /"\  u | \ |"|  U|"|u  
U | | u   |  _|"     | | |_|<\___ \/       \|  _ \/ \| |\| |  |_"_|  U | | u /| | | |      \/ _ \/      <\___ \/ <|  \| |>    | | | |\"\      /"/ \| |\/| |/ \/ _ \/ <|  \| |> \| |/  
 \| |/__  | |___    /| |\    u___) |        | |_) |  | |_| |   | |    \| |/__U| |_| |\     / ___ \       u___) | U| |\  |u.-,_| |_| |/\ \ /\ / /\  | |  | |  / ___ \ U| |\  |u  |_|   
  |_____| |_____|  u |_|U    |____/>>       |____/  <<\___/  U/| |\u   |_____||____/ u    /_/   \_\      |____/>> |_| \_|  \_)-\___/U  \ V  V /  U |_|  |_| /_/   \_\ |_| \_|   (_)   
  //  \\  <<   >>  _// \\_    )(  (__)     _|| \\_ (__) )(.-,_|___|_,-.//  \\  |||_        \\    >>       )(  (__)||   \\,-.    \\  .-,_\ /\ /_,-.<<,-,,-.   \\    >> ||   \\,-.|||_  
 (_")("_)(__) (__)(__) (__)  (__)         (__) (__)    (__)\_)-' '-(_/(_")("_)(__)_)      (__)  (__)     (__)     (_")  (_/    (__)  \_)-'  '-(_/  (./  \.) (__)  (__)(_")  (_/(__)_)

Objective:
Build the snowman! Player 1's job is to gather snowflakes to build the snowman,
but it's up to Player 2 to fend off the fireballs to preven the snowman from
melting. Unfortunately, every snowball you throw takes snow from the snowman as
well.

Player 1 -
	Flail around with the mouse to collect snowflakes

Player 2 -
	W,A,S,D 		- Move
	<^v> 			- Aim throw
	Space 			- Jump or Throw snowball if <^v> is held
	Joy Stick 0 	- Move
	Joy Stick 1		- Aim throw
	Button 9 or 5	- Throw snowball (corresponds to RB and Joy Stick 1 press on XBox 360 controller)
	Other Button	- Jump
	*Note: Throwing a snowball pushes you in the opposite direction. Try throwing down.

Thoughts:
I'm really more of a backend tech guy. I've got no head for game design. I did
my best to make something toddler + adult co-op friendly, but you be the judge.
That said, I'm only one player, so testing a a co-op game like this was rather
difficult. Probably needs balance tweaks. Definitely needs more interesting
game play, but, I really don't have much of a head for this stuff. Open to 
suggestions.

Min specs:
Pretty much any CPU should work.
GPU time hits about 12ms in the worst case on my 1060, so any modern card should be 
able to play somewhere in the [30, 60] Hz range. 

Tech notes:
The game assumes vsync and a 60 Hz display. All physics (movement) is done at 60 Hz
lockstep, but snowflake and fireball creation is done on frame counters. 
