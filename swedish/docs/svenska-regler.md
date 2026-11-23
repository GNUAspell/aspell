# Svenska språkregler och skrivregler – underlag för Hunspell och Aspell

Granskad 21 september 2026. Detta är ett källbelagt arbetsunderlag med 35 regelområden. Det beskriver ett urval av modern svenska, inte hela språkets grammatik. Testfall som är egna tillämpningar anges som sådana; källorna innehåller inte nödvändigtvis varje testord.

Den maskinläsbara katalogen finns i `docs/swedish-rules.json`. Ord- och kontextfall finns i `tests/swedish-rules.json`. Endast ordtesterna poängsätts i regressionstesterna; kontextfallen är exempel för framtida språkgranskning.

## Hur underlaget ska användas

Språkrådet (Isof) är normkälla för reglerna. Följ [projektets språkpolicy](sprakpolicy.md). Katalogens `dictionary_scope` skiljer ordtester från råd som kräver sammanhang.

Ordböckerna kan kontrollera ordformer och delar av ordbildningen. Grammatik kräver att ordens funktion i meningen är känd. Skrivregler behöver dessutom dokumenttyp, typografi och ibland skribentens avsikt. En utebliven stavningsmarkering bevisar därför inte att en mening eller ett datum är korrekt.

Språkrådets rekommendationer avser i första hand offentlig svenska. Vardaglig stil, dialekt, citat, historisk text och exakt återgiven kod kan motivera andra former. Stilpreferenser ska inte föras över till en ordlista som allmänna förbud. Se [Isofs beskrivning av Frågelådan](https://www.isof.se/svenska-spraket/frageladan).

## Regelkatalog

### R01. Tre lika konsonanter vid sammansättning

**Kontrollnivå: ordbildning.** När samma konsonant möts tre gånger vid en ordledsgräns skrivs normalt två. Ett bindestreck kan synliggöra gränsen; vid avstavning där återkommer den utelämnade konsonanten.

**Begränsning:** Regeln gäller konsonanter; flera vokaler får stå intill varandra. En teckensökning hittar även initialförkortningar, namn och uttrycksfull stavning som måste bedömas separat. Avstavning kräver egna tester.

**För ordboksarbetet:** Behåll positiva, negativa och bindestrecksförsedda testfall. Granska explicit lagrade fel som kan kringgå sammansättningsreglerna.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/31080).

### R02. Dubbelteckning av m och lexikala undantag

**Kontrollnivå: stavning och böjning.** Ett betonat kort vokalljud kan följas av dubbelt m framför en vokaländelse, även när grundordet slutar på enkelt m. Vissa grundord har ändå mm för att skiljas från ett annat ord.

**Begränsning:** Uttal och betydelse behövs: både dam och damm är riktiga ord. En generell regel som tar bort slutligt mm skulle förstöra giltiga ord.

**För ordboksarbetet:** Testa böjningsfamiljer och båda leden i betydelseskiljande ordpar.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/23280).

### R03. Enkelt n i böjda former på d och t

**Kontrollnivå: böjning.** I den undersökta ordfamiljen har hembränd och hembränt enkelt n, medan hembränning behåller dubbelteckningen.

**Begränsning:** Tillämpa på rätt morfologisk enhet. Följden nnt kan uppstå över en sammansättningsgräns: bränntemperatur är ett eget skyddsexempel, inte ett citat ur källan.

**För ordboksarbetet:** Testa både böjningsregeln och sammansättningar som inte ska normaliseras med en global teckenersättning.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/27957).

### R04. Dubbelteckning i avledda verb

**Kontrollnivå: avledning.** Grundordets betoning och vokallängd spelar roll. Formatera får ett t; etikettera behåller tt. Programmera får mm, medan reklamera har ett m.

**Begränsning:** Liknande ändelser innebär inte att samma dubblering passar alla stammar.

**För ordboksarbetet:** Kontrollera hela avledningsfamiljer; kopiera inte suffixflaggor mellan ord utan att kontrollera stammen.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/25353).

### R05. Accent i grundform och böjning

**Kontrollnivå: stavning och böjning.** Accenten följer med från en accentförsedd grundform till dess böjningar. Museum och jubileum ger däremot böjningar utan nytillkommen accent.

**Begränsning:** Accentbortfall kan ibland ge ett annat giltigt ord. Accenttecken ska därför inte avlägsnas eller läggas till globalt.

**För ordboksarbetet:** Testa separata böjningsklasser och kontrollera även inställningar som ignorerar accenter.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/24207), [Isof/Språkrådet 2](https://frageladan.isof.se/faqs/22554).

### R06. Foge-s är delvis lexikalt

**Kontrollnivå: ordbildning.** S används ofta efter en sammansatt förled eller vissa avledningsändelser. Men enkla förleder och flera etablerade sammansättningar måste bedömas ordvis. Ibland finns varianter med och utan s.

**Begränsning:** Ett korrekt foge-s försvinner inte automatiskt framför ytterligare s. Rekommenderad variant är inte alltid den enda korrekta.

**För ordboksarbetet:** Bygg belagda förledsklasser och behåll dokumenterade varianter. Ett generellt valfritt s skulle både godkänna fel och dölja struktur.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/30843), [Isof/Språkrådet 2](https://frageladan.isof.se/faqs/20670), [Isof/Språkrådet 3](https://frageladan.isof.se/faqs/26172).

### R07. Sammansättningens gruppering ändrar fogen

**Kontrollnivå: ordbildning.** Kärnkraftsindustri och kärnkraftverk grupperar delarna på olika sätt. Den närmaste ordledsstrukturen påverkar om s används.

**Begränsning:** Räkna inte bara antal delord. Ett känt helt efterled kan styra mer än en mekanisk regel för den första delen.

**För ordboksarbetet:** Testa kontrasterande ordbyggnad; prioritera morfologisk struktur framför teckenmönster.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/24435).

### R08. Fogevokaler och förändrad förled

**Kontrollnivå: ordbildning.** Förleder kan behålla, förlora eller byta slutvokal. Äldre och nyare ord samt olika sammansättningar med samma grundord kan bete sig olika.

**Begränsning:** Ladugård, kyrktorn och kyrkomusiker visar varför en enda ersättningsregel inte räcker. Dokumenterade parallellformer ska inte rensas bort.

**För ordboksarbetet:** Gruppera förleder efter faktiskt bruk; testa både etablerade former och tillåtna varianter.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/31251).

### R09. Bindestreck i förkortningssammansättningar

**Kontrollnivå: ordbildning och typografi.** Initialförkortningar i sammansättningar skrivs med bindestreck. Bindestreck kan även hjälpa när ett vanligt sammansatt ord annars blir svårtolkat.

**Begränsning:** Det innebär inte att alla sammansättningar med namn ska få bindestreck. Tokenisering kan göra att motorn bara granskar ordets delar.

**För ordboksarbetet:** Testa hela strängar via ord-API och visa separat vad kommandoradens textfilter gör.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/30528).

### R10. Egennamn följer inte alltid allmän ordbildning

**Kontrollnivå: namn och redaktionell norm.** Ett organisationsnamn kan ha en fastställd form som skiljer sig från den generiska benämningen. Namn med flera ord kan också bilda sammansättningar på annat sätt än enkla namn.

**Begränsning:** Svenska Fotbollförbundet är inte belägg för att det generiska ordet fotbollsförbund ska sakna s.

**För ordboksarbetet:** Håll namnbelägg åtskilda från produktiva regler och undvik automatiska namnändringar.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/29064), [Isof/Språkrådet 2](https://frageladan.isof.se/faqs/31227).

### R11. Genitiv och apostrof

**Kontrollnivå: morfologi och kontext.** Vanlig svensk genitiv bildas med s utan apostrof. Ord och namn som redan slutar på s, x eller z får normalt inget extra s.

**Begränsning:** Apostrof kan i särskilda fall förtydliga genitiv när huvudordet är utelämnat. Namn kan också ha avsiktlig särpräglad grafisk form.

**För ordboksarbetet:** Ge kontextbaserade råd; ett förbud mot alla apostrofer skulle vara fel.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/20976), [Isof/Språkrådet 2](https://frageladan.isof.se/faqs/30687).

### R12. Genitiv och böjning av förkortningar

**Kontrollnivå: morfologi och typografi.** Kolon kan skilja en bokstavsläst förkortning från dess böjningsändelse. Förkortningar som slutar på s kan få ett extra kolon-s för tydlighet men behöver det inte alltid.

**Begränsning:** SAS och SAS:s kan båda fungera i genitiv beroende på sammanhang. Samma behandling ska inte utan vidare användas för ord som utläses som vanliga ord.

**För ordboksarbetet:** Testa kolon som del av strängen och undvik att förväxla en godkänd token med en kontrollerad konstruktion.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/23874), [Isof/Språkrådet 2](https://frageladan.isof.se/faqs/21420).

### R13. Versaler i initialförkortningar

**Kontrollnivå: stil och lexikon.** Väl etablerade förkortningar för vanliga ord kan skrivas med gemener. Egennamn och mindre etablerade förkortningar behöver ofta versaler.

**Begränsning:** Förkortningens funktion och etablering styr. Varken versaler eller gemener ska normaliseras generellt.

**För ordboksarbetet:** Registrera vanliga former och låt en separat stilprofil styra rekommendationerna.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/21420).

### R14. Tillåten variation mellan ihop och isär

**Kontrollnivå: ord och fras.** Flera vanliga flerordsuttryck har både hopskrivna och särskrivna varianter. Konsekvens inom texten är ofta ett bättre råd än att förklara den ena formen fel.

**Begränsning:** I vissa uttryck påverkar skrivsättet betydelsen. Valfrihet för en grupp får inte generaliseras till alla uttryck.

**För ordboksarbetet:** Behåll tillåtna hopskrivna ord och testa uttryck som fraser i ett separat lager.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/23289).

### R15. Så här och såhär

**Kontrollnivå: ord och fras.** Både hopskrivna och särskrivna former av så här och så där accepteras; deras funktion och stil kan påverka vad som är vanligast.

**Begränsning:** Ett redaktionellt val av den vanligaste formen motiverar inte en hård svartlista.

**För ordboksarbetet:** Tillåt båda och ge eventuella stilråd på frasnivå.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/27942).

### R16. Sammansättning eller vanlig ordgrupp

**Kontrollnivå: betydelse och syntax.** Svenska sammansatta ord skrivs normalt ihop. Betoning, betydelse och hur leden böjs hjälper till att avgöra om det är ett sammansatt ord eller en vanlig ordgrupp.

**Begränsning:** Samma delord kan vara riktiga var för sig och ändå uttrycka något annat än det avsedda. En stavningsmotor har inte automatiskt tillgång till avsikten.

**För ordboksarbetet:** Testa kontraster med angiven betydelse i ett framtida grammatiklager; lägg inte alla separata ordpar i en förbudslista.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/21159).

### R17. Lånord och svensk plural

**Kontrollnivå: böjning och stil.** För policy rekommenderas plural på -er och bestämd plural på -erna. S-plural förekommer men kan göra bestämd plural svårare att uttrycka.

**Begränsning:** Policys kan också vara en vanlig singular genitiv. En ordlista kan därför inte generellt förbjuda formen för att en annan plural rekommenderas.

**För ordboksarbetet:** Testa rekommenderade böjningar och behåll morfologiskt giltig genitiv.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/21255).

### R18. Talord skrivna med bokstäver

**Kontrollnivå: ordbildning och skrivsätt.** Många talord skrivs ihop. Avrundade hundra- och tusental kan även delas för läsbarhet. I sammansättningar hålls talledet ihop; miljon och större enheter står som egna ord.

**Begränsning:** En rekommendation om läsbarhet är inte en regel att alla talord alltid ska delas eller alltid skrivas ihop.

**För ordboksarbetet:** Testa hela talord, tillåtna fraser och sammansättningar var för sig.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/27138).

### R19. Länder, språk och nationalitet

**Kontrollnivå: versaler och kontext.** Landsnamn skrivs med stor begynnelsebokstav, medan språk och nationella beteckningar normalt har liten bokstav.

**Begränsning:** Meningsbörjan och egennamnsanvändning kan ge andra versaler. Att ett ord med stor bokstav accepteras är därför inget bevis på fel i ordboken.

**För ordboksarbetet:** Kontrollera versaler i meningskontext och undvik hårda förbud mot möjliga meningsinledningar.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/29769), [Isof/Språkrådet 2](https://frageladan.isof.se/faqs/29850).

### R20. Varumärkets logotyp och namn i löptext

**Kontrollnivå: redaktionell stil.** Språkrådet rekommenderar ofta vanlig namnkapitalisering i löptext även när logotypen använder avvikande versaler.

**Begränsning:** Kodidentifierare, exakta produktsträngar och citat har andra krav. Normalisera inte dessa automatiskt.

**För ordboksarbetet:** Gör rådet valbart i en stilprofil, inte till ett generellt stavningsförbud.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/29223).

### R21. Decimalkomma

**Kontrollnivå: tal och typografi.** I vanlig svensk text avskiljs decimaler med komma.

**Begränsning:** Programkod, versionsnummer och vissa utbyten av maskindata använder andra konventioner.

**För ordboksarbetet:** Kontrollera tal i rätt dokumenttyp; ett numeriskt uttryck som ignoreras av stavningsmotorn är inte validerat.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/21402).

### R22. Gruppering av stora tal

**Kontrollnivå: tal och typografi.** Längre siffertal grupperas normalt i tre positioner räknat från höger. Ett fast mellanslag håller ihop talet vid radbrytning.

**Begränsning:** Identifierare, årtal och andra sifferkoder är inte vanliga antal som ska grupperas mekaniskt.

**För ordboksarbetet:** Använd dokumentanalys med taltyper och Unicodekontroll.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/30318).

### R23. Procent, enheter och fast mellanslag

**Kontrollnivå: tal och typografi.** Procenttecken skiljs från talet med mellanrum; fast mellanslag förebygger radbrytning. I löptext kan det utskrivna ordet vara tydligare.

**Begränsning:** Sammansatta adjektiv med procent följer andra mönster än ett tal följt av symbol.

**För ordboksarbetet:** Separera texttyp, mellanrumsregel och ordbildning.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/21726).

### R24. Tankstreck för intervall, bindestreck för ord

**Kontrollnivå: typografi.** Ett intervall mellan två tal kan skrivas med tankstreck utan omgivande mellanrum. Bindestreck binder i stället ihop ordled; samma uttryck kan därför innehålla båda tecknen.

**Begränsning:** Ungefärliga antal och intervall betyder inte alltid samma sak. Minustecken i matematik har ytterligare en funktion.

**För ordboksarbetet:** Kontrollera tecknets roll, inte bara dess utseende.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/22053), [Isof/Språkrådet 2](https://frageladan.isof.se/faqs/26553), [Isof/Språkrådet 3](https://frageladan.isof.se/faqs/29229).

### R25. Datum och valet mellan siffror och bokstäver

**Kontrollnivå: texttyp och typografi.** Valet mellan siffror och bokstäver beror på texttyp och sammanhang. Låga tal skrivs ofta med bokstäver när antalet inte är centralt; Myndigheternas skrivregler nämner ofta tolv eller lägre. Siffror används när taluppgiften är central och framför förkortade måttenheter. Månadens namn passar ofta i löptext; fullständigt numeriskt datum passar i vissa standardiserade uppgifter.

**Begränsning:** Det finns ingen exakt allmän gräns vid nio, tio eller tolv. Precision, mängden taluppgifter och konsekvens mellan jämförbara antal kan motivera siffror även för låga tal. Även stora ungefärliga tal kan skrivas med bokstäver.

**För ordboksarbetet:** Ge endast råd med hänsyn till sammanhanget. Ingen automatisk ersättning mellan siffror och bokstäver eller generell stavfelsmarkering. Kontrollera även att datum är entydiga.

Exempel i vanlig berättande text: ”Vi köpte sju böcker.” Vid mått används exempelvis ”7 kg”. Jämförbara antal kan hållas konsekventa: ”Grupperna hade 7, 14 och 21 deltagare.”

Källor: [Isof/Språkrådet – lärarhandledning](https://www.isof.se/download/18.5409ff0518d54a4bbada3de/1706690111224/L%C3%A4rarhandledning-Snabba-skrivregler2.pdf), [Myndigheternas skrivregler, avsnitt 12.1](https://sprakochfolkminnen.diva-portal.org/smash/get/diva2:1136028/FULLTEXT02.pdf). Rådet bedöms utifrån sammanhanget; projektet inför ingen egen siffergräns.

### R26. Kolon och semikolon har olika uppgifter

**Kontrollnivå: interpunktion.** Kolon kan inleda en förklaring eller uppräkning. Semikolon kan förbinda närstående satser eller skilja grupper inne i en uppräkning.

**Begränsning:** Semikolon är inte en dekorativ ersättning för kolon framför en uppräkning.

**För ordboksarbetet:** Kräver analys av satser och uppräkningsstruktur.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/21288).

### R27. Interpunktion lämnar utrymme för val

**Kontrollnivå: interpunktion och stil.** Skiljetecken uttrycker hur starka avbrott och samband skribenten vill visa. Flera lösningar kan vara möjliga mellan satser.

**Begränsning:** Alla avvikelser från en föredragen kommatering är inte språkliga fel.

**För ordboksarbetet:** Skilj motiverade råd från hårda grammatikfel i gränssnitt och testfacit.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/26862).

### R28. Avstavning och sammanhållna uttryck

**Kontrollnivå: layout.** Siffergrupper, förkortningar och liknande enheter bör hållas ihop. Namn och adresser behöver särskild försiktighet vid radbrytning.

**Begränsning:** Godkännande av en sträng med bindestreck bevisar inte att ett program kan avstava vid rätt ställe eller återställa en konsonant över ett radbyte.

**För ordboksarbetet:** Separat testsvit för layout och avstavningsmotor; håll isär hårt bindestreck, mjukt bindestreck och radbrytning.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/30765), [Isof/Språkrådet 2](https://frageladan.isof.se/faqs/31080).

### R29. De, dem och dom

**Kontrollnivå: syntax och stil.** De och dem rekommenderas i neutral standardskrift; dom kan passa en mer samtalsnära text. Valet mellan pronomenformer påverkas av satsfunktion och ibland efterföljande bestämningar.

**Begränsning:** De är också bestämd artikel. Dom är dessutom ett eget substantiv. Ingen av dessa former kan generellt förbjudas i en ordlista.

**För ordboksarbetet:** Använd grammatisk kontext och tillåt en vardaglig stilprofil. Undvik absoluta ersättningsregler framför som.

Källor: [Isof/Språkrådet 1](https://www.isof.se/svenska-spraket/klarsprak/bulletinen-klarsprak/artiklar-klarsprak/2022-05-23-sprakfragan-hur-gor-vi-med--dom--), [Isof/Språkrådet 2](https://frageladan.isof.se/faqs/30873).

### R30. Sin, sitt, sina och hans, hennes, deras

**Kontrollnivå: referens och syntax.** Reflexiva possessiva pronomen anknyter normalt till subjektet i samma sats. Andra possessiva pronomen kan anknyta till någon annan eller till ett subjekt i en annan sats.

**Begränsning:** Satsliknande konstruktioner och fasta uttryck ger accepterad variation. Exempelvis fungerar både i sin helhet och i dess helhet i vissa konstruktioner.

**För ordboksarbetet:** Granska referenter och satsgränser; automatisk ersättning av ord är olämplig.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/30957), [Isof/Språkrådet 2](https://frageladan.isof.se/faqs/23373), [Isof/Språkrådet 3](https://frageladan.isof.se/faqs/27570).

### R31. Kongruens: grammatisk form och betydelse

**Kontrollnivå: syntax.** Kollektiva substantiv kan med predikativa adjektiv följas av antingen grammatisk singular eller plural efter betydelsen. Framför substantivet gäller inte samma valfrihet.

**Begränsning:** Ett mekaniskt krav på singular efter varje kollektivt substantiv skulle ge falska larm.

**För ordboksarbetet:** Testa attributiv och predikativ användning separat och bevara accepterad variation.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/20997).

### R32. Bisatsordföljd och ledig stil

**Kontrollnivå: syntax och stil.** I neutral skrift står satsadverbial normalt före det finita verbet i en bisats. Huvudsatsordföljd förekommer däremot i vissa bisatser i samtal och ledigare skrift.

**Begränsning:** Den vanliga undervisningstumregeln beskriver inte alla konstruktioner och stilar. Att flytta en negation kan också förändra vad den hör till.

**För ordboksarbetet:** Bygg stilmedvetna, satsbaserade kontroller och undvik ordpositionsregler utan parsning.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/21486).

### R33. Rättstavat ord med fel betydelse

**Kontrollnivå: lexikal betydelse.** Sträck och streck är båda giltiga ord men används i olika uttryck och betydelser.

**Begränsning:** Att lägga båda i en ordlista löser inte valet mellan dem i en mening.

**För ordboksarbetet:** Testa fasta uttryck och betydelsebärande kontext i ett språkgranskningslager.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/27078).

### R34. Sammansättningar med flerordig förled

**Kontrollnivå: ordbildning och typografi.** När en fras ingår som förled kan bindestreck markera anslutningen till huvudledet. Längre och mindre etablerade fraser behöver ofta tydligare sammanbindning.

**Begränsning:** Väl etablerade uttryck kan ibland bilda ett enda sammanhängande ord. Samma lösning passar inte varje förled.

**För ordboksarbetet:** Behåll hela frasen i analysen; vanlig ordvis tokenisering tappar relationerna.

Källor: [Isof/Språkrådet 1](https://frageladan.isof.se/faqs/20742).

### R35. Genus och böjningsklass är lexikala uppgifter

**Kontrollnivå: böjning.** Språkrådet hänvisar till SAOL för enskilda ords böjning (Myndigheternas skrivregler, avsnitt 6.1). Ett substantivs genus och betydelse påverkar böjningen. Ordet spann förekommer exempelvis i olika betydelser med olika genus och böjningsmönster.

**Begränsning:** Stammens sista bokstav räcker inte för att välja suffix. Den tidigare flaggningen enspann/C gav ett extra n i den lokala ordboken: detta är vår diagnos av en flaggning, inte en ny allmän svensk språkregel.

**För ordboksarbetet:** Granska hela böjningsparadigmet när en flagga tilldelas. Testa både rätt former och former som en felvald klass skulle generera.

Källor: [Språkrådet, Myndigheternas skrivregler, avsnitt 6.1](https://sprakochfolkminnen.diva-portal.org/smash/get/diva2:1136028/FULLTEXT02.pdf), [SAOL: spann](https://svenska.se/saol/?hv=xnr464207).

## Prioritering för fortsatt implementation

1. Granska explicit lagrade stavfel och affix som genererar felaktiga former. Börja med tydliga fel med källbelagt facit. Lägg till både positiva och negativa regressionsexempel.

2. Rätta täckningsluckor för accepterade varianter. En rekommenderad huvudform får inte leda till att andra tillåtna former rensas bort.

3. Utveckla sammansättningar med belagda förledsklasser och fogar. Aspell-exportens ändliga ordlista kan inte representera alla nya sammansättningar som Hunspell kan bilda.

4. Lägg kontextberoende grammatik, stil, talformat och avstavning i separata komponenter. Märk deras råd med regel, källa, stilprofil och osäkerhet.

## Vad som återstår att undersöka

Ljudenlig stavning och dess undantag (sj-, tj-, j- och ng-ljud), starka verb, particip, adjektivkomparation, numerus- och genusvariation, prepositionsbruk, finlandssvensk standardsvenska, dialekter, historiska stavningar och större namn- och fackordsmängder behöver egna undersökningar. För uttalsberoende regler krävs uttals- eller lexikala uppgifter; de kan inte säkert härledas ur bokstavsföljden ensam.
