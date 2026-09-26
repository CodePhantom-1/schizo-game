# Invented ledger — P0a, the item catalogue's physical columns (FND-02)

Per D-018: authored glue the game needs, tagged `INVENTED` in spirit (the canon rows keep their own tags; only these five new columns are glue), fitting theme and canon, vetoable (a veto supersedes a line here). No canon value was changed; the columns were appended.

| Choice | Value | Why |
|---|---|---|
| The UI categories | food, drink, ingredient, material, tool, weapon, ammunition, armour, shield, clothing, ritual, document, trade_good, station_part, animal, medicine, treasure | The 34 free-text `category` values can't drive an inventory filter; `category` stays because the needs table reads it |
| A unit of grain or wheat | 800 g, stacks of 50, never spoils | About one sila (≈1 litre) of barley; stored grain's losses are pests and damp (PRP-13), not spoilage |
| Fish | 500 g, spoils in 1 day | Fresh fish in the southern heat (B7: "fish rots in a day") |
| Bread | 250 g, spoils in 3 days | A flat barley loaf |
| Beer | 1000 g, spoils in 5 days | A jar-measure of unfiltered barley beer |
| Dates, flour, malt, beer-bread | 60, 90, 90, 30 days | Dried fruit and dry goods keep; bappir is part-baked |
| Water | 1000 g a unit (1 litre) | Carried water weighs; well water drunk on the spot is not held |
| Ritual and documents | tablets 200–400 g (flag `document`), scrolls 300 g, amulets 30 g, lots 20 g | Real sizes of clay tablets, liver models and small charms |
| Metals and treasure | copper ingot 2 kg, tin 1 kg, gold 10 g a piece, gems 50–100 g | Trade-sized pieces, not whole oxhide ingots |
| Stations | quern 25 kg, oven 80 kg, vat 30 kg, potter's wheel 40 kg, loom 20 kg, smithing hearth 100 kg; flag `fixture` | They are fixtures of a workshop; the flag tells the UI they are not normally carried |
| The pack donkey | 150 kg, flag `animal` | A living animal; it is led, not carried (ANM-01) |
| Weapons, armour, shields | the `arms.csv` `weight_g` | One source of truth for arms weight |
| Arrows and sling bullets | 25 g and 60 g, stacks of 50 | A reed arrow; a fired-clay sling bullet (the Hamoukar bullets) |
| Medicine | herbs 100 g (spoil 30 days), bandage 50 g | |
| Carrying capacity (FND-03) | 30 kg + 3 kg per Strength point; burdened past 100%, pinned past 125%; silver weighs 8 g a shekel of 180 grains | A laden porter's day load; the shekel's weight |
