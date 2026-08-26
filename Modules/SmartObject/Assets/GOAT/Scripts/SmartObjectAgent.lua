-- A worked example for the GOAT_SmartObject gem.
--
-- Uses only this gem's words and the genre neutral core, so it compiles with no other module
-- enabled. A real agent walks to the object in between claiming and using it -- see the gem's
-- README for that shape, which needs GOAT_Navigation or the project's own controller.

return tree "SmartObjectAgent" {
    selector {
        -- Take a seat, settle into it for a while, then go looking for the next thing.
        -- The slot is held from the claim until use_smart_object ends, aborts included.
        sequence {
            claim_smart_object "sit" { radius = 25 },
            use_smart_object { seconds = 3 },
        },

        -- Nothing free within range: pause rather than re-scanning every tick.
        wait(2.0),
    },
}
