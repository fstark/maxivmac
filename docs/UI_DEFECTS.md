* Cosmetic: initial choice screen is not really nices

The inital choice screen have scrollable zones for the Mac models. It should not be scrollable.
The background is some sort of gray/blue. I don't this there should be a different background
I think ithe while screen should be of a light gtay, maybe even a 50% gray to give the "Mac Boot" feel to that UI (without goint to the 1x1 checkerboard)
Same issue in the "more models", althought it is worse because there is no info after the name. More models should reuse the exact same display code as the inital choice, just with more data. It could even be the exact same screen, just a toggle.

* Major: Window not linked to actual mac size

The window in windowed mode should be exactly the size of the selected Mac, to reproduce the feeling of the original emulator.

* Major: Two mouse cursors in windowed mode

In windowed move, we can see the host cursor and the emulated cursor when moving the mouse of the emulator content. Host mouse must disapear, unless in "control" mode.

* Critical: Impossible to access the control mode

Accessing the contol mode by pressing Ctrl does not work. I acceidentaly enabled it once, but I am not able to reproduce this.

* Maybe: Control mode should disapear when key is released

In windows mode, I would like to try to only have the control ui displayed when the control key is pressed, so the ui would be very unintrusive.

* Emulated UI should not be movable

Today, in windowed mode, if I click on the background, I can move the emulated mac around inside the window. That is wrong.
