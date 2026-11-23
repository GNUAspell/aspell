# Språkrådet som normkälla

Språkrådet vid Institutet för språk och folkminnen (Isof) är projektets
förstahandskälla för svenska språkregler och skrivregler. Denna policy gäller
ordboksändringar, regelunderlag, rättningsförslag och tester.

## Källor och bedömning

- Slå alltid upp nya eller ändrade regler hos Språkrådet, i första hand i
  [Frågelådan](https://frageladan.isof.se/), och använd relevanta avsnitt i
  *Svenska skrivregler*, *Snabba skrivregler* eller
  [Myndigheternas skrivregler](https://www.isof.se/utforska/vagledningar/myndigheternas-skrivregler).
  Kontrollera publicering, uppdatering och vilken texttyp rådet gäller.
- Dokumentera den direkta källan, avsnittet vid behov och datum för granskning.
  Återge rekommendationen med egna ord. Skilj källans exempel från egna
  tillämpningar. Importera inte hela publikationer som regeldata.
- Låt Språkrådets aktuella, relevanta råd styra. Om två råd skiljer sig åt,
  utred texttyp, sammanhang och aktualitet; välj inte automatiskt det strängare.
  Saknas tydligt stöd, dokumentera osäkerheten och avstå från en generell rättning.
- Använd SAOL som komplement för enskilda ords stavning, genus och böjning,
  enligt Språkrådets hänvisning i *Myndigheternas skrivregler*, avsnitt 5.1 och
  6.1. En rekommenderad variant gör inte andra accepterade varianter till stavfel.
  Frekvens i översättningsminnen är inte i sig normstöd.

## Tillämpning i verktygen

`swedish-rules.json` är det granskade regelunderlaget. Varje regel har en
Språkrådskälla och `dictionary_scope`: `word` medger granskade ordtester och
ordrättningar; `context` kräver analys av sammanhanget. Även en regel med
`word` kan ha begränsningar som hindrar en generell ersättning.

`lexical-corrections.json` innehåller enbart granskade ordformer. Rättningarna
och testfallen hänvisar till regelns registrerade källor, inklusive eventuellt
kompletterande lexikonstöd. Kontextfall får inte ha `expected_accept` och ska
inte poängsättas som stavning. Hunspell och Aspell bedömer ord, inte hela
meningars grammatik eller lämplig stil.

Kör `python3 tools/check_language_policy.py` från projektets ordbokskatalog.
Kontrollen ingår även i regressionstesterna. Den kontrollerar källhänvisningar
och kontrollnivåer offline; den kan inte avgöra om en källa är rätt tolkad eller
fortfarande aktuell. Källan behöver läsas och bedömas inför en regeländring.

### Tal med siffror eller bokstäver

R25 följer Språkrådets kontextberoende råd. Låga tal kan skrivas med bokstäver
när antalet är mindre centralt; sifferbetonade uppgifter och förkortade enheter
talar för siffror. Jämförbara tal bör hanteras konsekvent. Projektet föreskriver
ingen automatisk gräns vid nio, tio eller tolv. Se regelns källor och exempel.
