#include <assert.h>
#include <sos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define SMALL_BUF_SZ 2
#define BUF_SZ 5000
static char test_str[] =
	"AAA%AAsAABAA$AAnAACAA-AA(AADAA;AA)"
	"AAEAAaAA0AAFAAbAA1AAGAAcAA2AAHAAdAA3AAIAAeAA4AAJAAfAA5AAKAAgAA6AALAAhAA7AA"
	"MAAiAA8AANAAjAA9AAOAAkAAPAAlAAQAAmAARAAoAASAApAATAAqAAUAArAAVAAtAAWAAuAAXA"
	"AvAAYAAwAAZAAxAAyAAzA%%A%sA%BA%$A%nA%CA%-A%(A%DA%;A%)A%EA%aA%0A%FA%bA%1A%"
	"GA%cA%2A%HA%dA%3A%IA%eA%4A%JA%fA%5A%KA%gA%6A%LA%hA%7A%MA%iA%8A%NA%jA%9A%"
	"OA%kA%PA%lA%QA%mA%RA%oA%SA%pA%TA%qA%UA%rA%VA%tA%WA%uA%XA%vA%YA%wA%ZA%xA%"
	"yA%zAs%AssAsBAs$AsnAsCAs-As(AsDAs;As)"
	"AsEAsaAs0AsFAsbAs1AsGAscAs2AsHAsdAs3AsIAseAs4AsJAsfAs5AsKAsgAs6AsLAshAs7As"
	"MAsiAs8AsNAsjAs9AsOAskAsPAslAsQAsmAsRAsoAsSAspAsTAsqAsUAsrAsVAstAsWAsuAsXA"
	"svAsYAswAsZAsxAsyAszAB%ABsABBAB$ABnABCAB-AB(ABDAB;AB)"
	"ABEABaAB0ABFABbAB1ABGABcAB2ABHABdAB3ABIABeAB4ABJABfAB5ABKABgAB6ABLABhAB7AB"
	"MABiAB8ABNABjAB9ABOABkABPABlABQABmABRABoABSABpABTABqABUABrABVABtABWABuABXA"
	"BvABYABwABZABxAByABzA$%A$sA$BA$$A$nA$CA$-A$(A$DA$;A$)A$EA$aA$0A$FA$bA$1A$"
	"GA$cA$2A$HA$dA$3A$IA$eA$4A$JA$fA$5A$KA$gA$6A$LA$hA$7A$MA$iA$8A$NA$jA$9A$"
	"OA$kA$PA$lA$QA$mA$RA$oA$SA$pA$TA$qA$UA$rA$VA$tA$WA$uA$XA$vA$YA$wA$ZA$xA$"
	"yA$zAn%AnsAnBAn$AnnAnCAn-An(AnDAn;An)"
	"AnEAnaAn0AnFAnbAn1AnGAncAn2AnHAndAn3AnIAneAn4AnJAnfAn5AnKAngAn6AnLAnhAn7An"
	"MAniAn8AnNAnjAn9AnOAnkAnPAnlAnQAnmAnRAnoAnSAnpAnTAnqAnUAnrAnVAntAnWAnuAnXA"
	"nvAnYAnwAnZAnxAnyAnzAC%ACsACBAC$ACnACCAC-AC(ACDAC;AC)"
	"ACEACaAC0ACFACbAC1ACGACcAC2ACHACdAC3ACIACeAC4ACJACfAC5ACKACgAC6ACLAChAC7AC"
	"MACiAC8ACNACjAC9ACOACkACPAClACQACmACRACoACSACpACTACqACUACrACVACtACWACuACXA"
	"CvACYACwACZACxACyACzA-%A-sA-BA-$A-nA-CA--A-(A-DA-;A-)A-EA-aA-0A-FA-bA-1A-"
	"GA-cA-2A-HA-dA-3A-IA-eA-4A-JA-fA-5A-KA-gA-6A-LA-hA-7A-MA-iA-8A-NA-jA-9A-"
	"OA-kA-PA-lA-QA-mA-RA-oA-SA-pA-TA-qA-UA-rA-VA-tA-WA-uA-XA-vA-YA-wA-ZA-xA-"
	"yA-zA(%A(sA(BA($A(nA(CA(-A((A(DA(;A()A(EA(aA(0A(FA(bA(1A(GA(cA(2A(HA(dA("
	"3A(IA(eA(4A(JA(fA(5A(KA(gA(6A(LA(hA(7A(MA(iA(8A(NA(jA(9A(OA(kA(PA(lA(QA("
	"mA(RA(oA(SA(pA(TA(qA(UA(rA(VA(tA(WA(uA(XA(vA(YA(wA(ZA(xA(yA(zAD%ADsADBAD$"
	"ADnADCAD-AD(ADDAD;AD)"
	"ADEADaAD0ADFADbAD1ADGADcAD2ADHADdAD3ADIADeAD4ADJADfAD5ADKADgAD6ADLADhAD7AD"
	"MADiAD8ADNADjAD9ADOADkADPADlADQADmADRADoADSADpADTADqADUADrADVADtADWADuADXA"
	"DvADYADwADZADxADyADzA;%A;sA;BA;$A;nA;CA;-A;(A;DA;;A;)A;EA;aA;0A;FA;bA;1A;"
	"GA;cA;2A;HA;dA;3A;IA;eA;4A;JA;fA;5A;KA;gA;6A;LA;hA;7A;MA;iA;8A;NA;jA;9A;"
	"OA;kA;PA;lA;QA;mA;RA;oA;SA;pA;TA;qA;UA;rA;VA;tA;WA;uA;XA;vA;YA;wA;ZA;xA;"
	"yA;zA)%A)sA)BA)$A)nA)CA)-A)(A)DA);A))A)EA)aA)0A)FA)bA)1A)GA)cA)2A)HA)dA)"
	"3A)IA)eA)4A)JA)fA)5A)KA)gA)6A)LA)hA)7A)MA)iA)8A)NA)jA)9A)OA)kA)PA)lA)QA)"
	"mA)RA)oA)SA)pA)TA)qA)UA)rA)VA)tA)WA)uA)XA)vA)YA)wA)ZA)xA)yA)zAE%AEsAEBAE$"
	"AEnAECAE-AE(AEDAE;AE)"
	"AEEAEaAE0AEFAEbAE1AEGAEcAE2AEHAEdAE3AEIAEeAE4AEJAEfAE5AEKAEgAE6AELAEhAE7AE"
	"MAEiAE8AENAEjAE9AEOAEkAEPAElAEQAEmAERAEoAESAEpAETAEqAEUAErAEVAEtAEWAEuAEXA"
	"EvAEYAEwAEZAExAEyAEzAa%AasAaBAa$AanAaCAa-Aa(AaDAa;Aa)"
	"AaEAaaAa0AaFAabAa1AaGAacAa2AaHAadAa3AaIAaeAa4AaJAafAa5AaKAagAa6AaLAahAa7Aa"
	"MAaiAa8AaNAajAa9AaOAakAaPAalAaQAamAaRAaoAaSAapAaTAaqAaUAarAaVAatAaWAauAaXA"
	"avAaYAawAaZAaxAayAazA0%A0sA0BA0$A0nA0CA0-A0(A0DA0;A0)"
	"A0EA0aA00A0FA0bA01A0GA0cA02A0HA0dA03A0IA0eA04A0JA0fA05A0KA0gA06A0LA0hA07A0"
	"MA0iA08A0NA0jA09A0OA0kA0PA0lA0QA0mA0RA0oA0SA0pA0TA0qA0UA0rA0VA0tA0WA0uA0XA"
	"0vA0YA0wA0ZA0xA0yA0zAF%AFsAFBAF$AFnAFCAF-AF(AFDAF;AF)"
	"AFEAFaAF0AFFAFbAF1AFGAFcAF2AFHAFdAF3AFIAFeAF4AFJAFfAF5AFKAFgAF6AFLAFhAF7AF"
	"MAFiAF8AFNAFjAF9AFOAFkAFPAFlAFQAFmAFRAFoAFSAFpAFTAFqAFUAFrAFVAFtAFWAFuAFXA"
	"FvAFYAFwAFZAFxAFyAFzAb%AbsAbBAb$AbnAbCAb-Ab(AbDAb;Ab)"
	"AbEAbaAb0AbFAbbAb1AbGAbcAb2AbHAbdAb3AbIAbeAb4AbJAbfAb5AbKAbgAb6AbLAbhAb7Ab"
	"MAbiAb8AbNAbjAb9AbOAbkAbPAblAbQAbmAbRAboAbSAbpAbTAbqAbUAbrAbVAbtAbWAbuAbXA"
	"bvAbYAbwAbZAbxAbyAbzA1%A1sA1BA1$A1nA1CA1-A1(A1DA1;A1)"
	"A1EA1aA10A1FA1bA11A1GA1cA12A1HA1dA13A1IA1eA14A1JA1fA15A1KA1gA16A1LA1hA17A1"
	"MA1iA18A1NA1jA19A1OA1kA1PA1lA1QA1mA1RA1oA1SA1pA1TA1qA1UA1rA1VA1tA1WA1uA1XA"
	"1vA1YA1wA1ZA1xA1yA1zAG%AGsAGBAG$AGnAGCAG-AG(AGDAG;AG)"
	"AGEAGaAG0AGFAGbAG1AGGAGcAG2AGHAGdAG3AGIAGeAG4AGJAGfAG5AGKAGgAG6AGLAGhAG7AG"
	"MAGiAG8AGNAGjAG9AGOAGkAGPAGlAGQAGmAGRAGoAGSAGpAGTAGqAGUAGrAGVAGtAGWAGuAGXA"
	"GvAGYAGwAGZAGxAGyAGzAc%AcsAcBAc$AcnAcCAc-Ac(AcDAc;Ac)"
	"AcEAcaAc0AcFAcbAc1AcGAccAc2AcHAcdAc3AcIAceAc4AcJAcfAc5AcKAcgAc6AcLAchAc7Ac"
	"MAciAc8AcNAcjAc9AcOAckAcPAclAcQAcmAcRAcoAcSAcpAcTAcqAcUAcrAcVActAcWAcuAcXA"
	"cvAcYAcwAcZAcxAcyAczA2%A2sA2BA2$A2nA2CA2-A2(A2DA2;A2)"
	"A2EA2aA20A2FA2bA21A2GA2cA22A2HA2dA23A2IA2eA24A2JA2fA25A2KA2gA26A2LA2hA27A2"
	"MA2iA28A2NA2jA29A2OA2kA2PA2lA2QA2mA2RA2oA2SA2pA2TA2qA2UA2rA2VA2tA2WA2uA2XA"
	"2vA2YA2wA2ZA2xA2yA2zAH%AHsAHBAH$AHnAHCAH-AH(AHDAH;AH)"
	"AHEAHaAH0AHFAHbAH1AHGAHcAH2AHHAHdAH3AHIAHeAH4AHJAHfAH5AHKAHgAH6AHLAHhAH7AH"
	"MAHiAH8AHNAHjAH9AHOAHkAHPAHlAHQAHmAHRAHoAHSAHpAHTAHqAHUAHrAHVAHtAHWAHuAHXA"
	"HvAHYAHwAHZAHxAHyAHzAd%AdsAdBAd$AdnAdCAd-Ad(AdDAd;Ad)"
	"AdEAdaAd0AdFAdbAd1AdGAdcAd2AdHAddAd3AdIAdeAd4AdJAdfAd5AdKAdgAd6AdLAdhAd7Ad"
	"MAdiAd8AdNAdjAd9AdOAdkAdPAdlAdQAdmAdRAdoAdSAdpAdTAdqAdUAdrAdVAdtAdWAduAdXA"
	"dvAdYAdwAdZAdxAdyAdzA3%A3sA3BA3$A3nA3CA3-A3(A3DA3;A3)"
	"A3EA3aA30A3FA3bA31A3GA3cA32A3HA3dA33A3IA3eA34A3JA3fA35A3KA3gA36A3LA3hA37A3"
	"MA3iA38A3NA3jA39A3OA3kA3PA3lA3QA3mA3RA3oA3SA3pA3TA3qA3UA3rA3VA3tA3WA3uA3XA"
	"3vA3YA3wA3ZA3xA3yA3zAI%AIsAIBAI$AInAICAI-AI(AIDAI;AI)"
	"AIEAIaAI0AIFAIbAI1AIGAIcAI2AIHAIdAI3AIIAIeAI4AIJAIfAI5AIKAIgAI6AILAIhAI7AI"
	"MAIiAI8AINAIjAI9AIOAIkAIPAIlAIQAImAIRAIoAISAIpAITAIqAIUAIrAIVAItAIWAIuAIXA"
	"IvAIYAIwAIZAIxAIyAIzAe%AesAeBAe$AenAeCAe-Ae(AeDAe;Ae)"
	"AeEAeaAe0AeFAebAe1AeGAecAe2AeHAedAe3AeIAeeAe4AeJAefAe5AeKAegAe6AeLAehAe7Ae"
	"MAeiAe8AeNAejAe9AeOAekAePAelAeQAemAeRAeoAeSAepAeTAeqAeUAerAeVAetAeWAeuAeXA"
	"evAeYAewAeZAexAeyAezA4%A4sA4BA4$A4nA4CA4-A4(A4DA4;A4)"
	"A4EA4aA40A4FA4bA41A4GA4cA42A4HA4dA43A4IA4eA44A4JA4fA45A4KA4gA46A4LA4hA47A4"
	"MA4iA48A4NA4jA49A4OA4kA4PA4lA4QA4mA4RA4oA4SA4pA4TA4qA4UA4rA4VA4tA4WA4uA4XA"
	"4vA4YA4wA4ZA4xA4yA4zAJ%AJsAJBAJ$AJnAJCAJ-AJ(AJDAJ;AJ)"
	"AJEAJaAJ0AJFAJbAJ1AJGAJcAJ2AJHAJdAJ3AJIAJeAJ4AJJAJfAJ5AJKAJgAJ6AJLAJhAJ7AJ"
	"MAJiAJ8AJNAJjAJ9AJOAJkAJPAJlAJQAJmAJRAJoAJSAJpAJTAJqAJUAJrAJVAJtAJWAJuAJXA"
	"JvAJYAJwAJZAJxAJyAJzAf%AfsAfBAf$AfnAfCAf-Af(AfDAf;Af)"
	"AfEAfaAf0AfFAfbAf1AfGAfcAf2AfHAfdAf3AfIAfeAf4AfJAffAf5AfKAfgAf6AfLAfhAf7Af"
	"MAfiAf8AfNAfjAf9AfOAfkAfPAflAfQAfmAfRAfoAfSAfpAfTAfqAfUAfrAfVAftAfWAfuAfXA"
	"fvAfYAfwAfZAfxAfyAfzA5%A5sA5BA5$A5nA5CA5-A5(A5DA5;A5)"
	"A5EA5aA50A5FA5bA51A5GA5cA52A5HA5dA53A5IA5eA54A5JA5fA55A5KA5gA56A5LA5hA57A5"
	"MA5iA58A5NA5jA59A5OA5kA5PA5lA5QA5mA5RA5oA5SA5pA5TA5qA5UA5rA5VA5tA5WA5uA5XA"
	"5vA5YA5wA5ZA5xA5yA5zAK%AKsAKBAK$AKnAKCAK-AK(AKDAK;AK)"
	"AKEAKaAK0AKFAKbAK1AKGAKcAK2AKHAKdAK3AKIAKeAK4AKJAKfAK5AKKAKgAK6AKLAKhAK7AK"
	"MAKiAK8AKNAKjAK9AKOAKkAKPAKlAKQAKmAKRAKoAKSAKpAKTAKqAKUAKrAKVAKtAKWAKuAKXA"
	"KvAKYAKwAKZAKxAKyAKzAg%AgsAgBAg$AgnAgCAg-Ag(AgDAg;Ag)"
	"AgEAgaAg0AgFAgbAg1AgGAgcAg2AgHAgdAg3AgIAgeAg4AgJAgfAg5AgKAggAg6AgLAghAg7Ag"
	"MAgiAg8AgNAgjAg9AgOAgkAgPAglAgQAgmAgRAgoAgSAgpAgTAgqAgUAgrAgVAgtAgWAguAgXA"
	"gvAgYAgwAgZAgxAgyAgzA6%A6sA6BA6$A6nA6CA6-A6(A6DA6;A6)"
	"A6EA6aA60A6FA6bA61A6GA6cA62A6HA6dA63A6IA6eA64A6JA6fA65A6KA6gA66A6LA6hA67A6"
	"MA6iA68A6NA6jA69A6OA6kA6PA6lA6QA6mA6RA6oA6SA6pA6TA6qA6UA6rA6VA6tA6WA6uA6XA"
	"6vA6YA6wA6ZA6xA6yA6zAL%ALsALBAL$ALnALCAL-AL(ALDAL;AL)"
	"ALEALaAL0ALFALbAL1ALGALcAL2ALHALdAL3ALIALeAL4ALJALfAL5ALKALgAL6ALLALhAL7AL"
	"MALiAL8ALNALjAL9ALOALkALPALlALQALmALRALoALSALpALTALqALUALrALVALtALWALuALXA"
	"LvALYALwALZALxALyALzAh%AhsAhBAh$AhnAhCAh-Ah(AhDAh;Ah)"
	"AhEAhaAh0AhFAhbAh1AhGAhcAh2AhHAhdAh3AhIAheAh4AhJAhfAh5AhKAhgAh6AhLAhhAh7Ah"
	"MAhiAh8AhNAhjAh9AhOAhkAhPAhlAhQAhmAhRAhoAhSAhpAhTAhqAhUAhrAhVAhtAhWAhuAhXA"
	"hvAhYAhwAhZAhxAhyAhzA7%A7sA7BA7$A7nA7CA7-A7(A7DA7;A7)"
	"A7EA7aA70A7FA7bA71A7GA7cA72A7HA7dA73A7IA7eA74A7JA7fA75A7KA7gA76A7LA7hA77A7"
	"MA7iA78A7NA7jA79A7OA7kA7PA7lA7QA7mA7RA7oA7SA7pA7TA7qA7UA7rA7VA7tA7WA7uA7XA"
	"7vA7YA7wA7ZA7xA7yA7zAM%AMsAMBAM$AMnAMCAM-AM(AMDAM;AM)"
	"AMEAMaAM0AMFAMbAM1AMGAMcAM2AMHAMdAM3AMIAMeAM4AMJAMfAM5AMKAMgAM6AMLAMhAM7AM"
	"MAMiAM8AMNAMjAM9AMOAMkAMPAMlAMQAMmAMRAMoAMSAMpAMTAMqAMUAMrAMVAMtAMWAMuAMXA"
	"MvAMYAMwAMZAMxAMyAMzAi%AisAiBAi$AinAiCAi-Ai(AiDAi;Ai)"
	"AiEAiaAi0AiFAibAi1AiGAicAi2AiHAidAi3AiIAieAi4AiJAifAi5AiKAigAi6AiLAihAi7Ai"
	"MAiiAi8AiNAijAi9AiOAikAiPAilAiQAimAiRAioAiSAipAiTAiqAiUAirAiVAitAiWAiuAiXA"
	"ivAiYAiwAiZAixAiyAizA8%A8sA8BA8$A8nA8CA8-A8(A8DA8;A8)"
	"A8EA8aA80A8FA8bA81A8GA8cA82A8HA8dA83A8IA8eA84A8JA8fA85A8KA8gA86A8LA8hA87A8"
	"MA8iA88A8NA8jA89A8OA8kA8PA8lA8QA8mA8RA8oA8SA8pA8TA8qA8UA8rA8VA8tA8WA8uA8XA"
	"8vA8YA8wA8ZA8xA8yA8zAN%ANsANBAN$ANnANCAN-AN(ANDAN;AN)"
	"ANEANaAN0ANFANbAN1ANGANcAN2ANHANdAN3ANIANeAN4ANJANfAN5ANKANgAN6ANLANhAN7AN"
	"MANiAN8ANNANjAN9ANOANkANPANlANQANmANRANoANSANpANTANqANUANrANVANtANWANuANXA"
	"NvANYANwANZANxANyANzAj%AjsAjBAj$AjnAjCAj-Aj(AjDAj;Aj)"
	"AjEAjaAj0AjFAjbAj1AjGAjcAj2AjHAjdAj3AjIAjeAj4AjJAjfAj5AjKAjgAj6AjLAjhAj7Aj"
	"MAjiAj8AjNAjjAj9AjOAjkAjPAjlAjQAjmAjRAjoAjSAjpAjTAjqAjUAjrAjVAjtAjWAjuAjXA"
	"jvAjYAjwAjZAjxAjyAjzA9%A9sA9BA9$A9nA9CA9-A9(A9DA9;A9)"
	"A9EA9aA90A9FA9bA91A9GA9cA92A9HA9dA93A9IA9eA94A9JA9fA95A9KA9gA96A9LA9hA97A9"
	"MA9iA98A9NA9jA99A9OA9kA9PA9lA9QA9mA9RA9oA9SA9pA9TA9qA9UA9rA9VA9tA9WA9uA9XA"
	"9vA9YA9wA9ZA9xA9yA9zAO%AOsAOBAO$AOnAOCAO-AO(AODAO;AO)"
	"AOEAOaAO0AOFAObAO1AOGAOcAO2AOHAOdAO3AOIAOeAO4AOJAOfAO5AOKAOgAO6AOLAOhAO7AO"
	"MAOiAO8AONAOjAO9AOOAOkAOPAOlAOQAOmAORAOoAOSAOpAOTAOqAOUAOrAOVAOtAOWAOuAOXA"
	"OvAOYAOwAOZAOxAOyAOzAk%AksAkBAk$AknAkCAk-Ak(AkDAk;Ak)"
	"AkEAkaAk0AkFAkbAk1AkGAkcAk2AkHAkdAk3AkIAkeAk4AkJAkfAk5AkKAkgAk6AkLAkhAk7Ak"
	"MAkiAk8AkNAkjAk9AkOAkkAkPAklAkQAkmAkRAkoAkSAkpAkTAkqAkUAkrAkVAktAkWAkuAkXA"
	"kvAkYAkwAkZAkxAkyAkzAP%APsAPBAP$APnAPCAP-AP(APDAP;AP)"
	"APEAPaAP0APFAPbAP1APGAPcAP2APHAPdAP3APIAPeAP4APJAPfAP5APKAPgAP6APLAPhAP7AP"
	"MAPiAP8APNAPjAP9APOAPkAPPAPlAPQAPmAPRAPoAPSAPpAPTAPqAPUAPrAPVAPtAPWAPuAPXA"
	"PvAPYAPwAPZAPxAPyAPzAl%AlsAlBAl$AlnAlCAl-Al(AlDAl;Al)"
	"AlEAlaAl0AlFAlbAl1AlGAlcAl2AlHAldAl3AlIAleAl4AlJAlfAl5AlKAlgAl6AlLAlhAl7Al"
	"MAliAl8AlNAljAl9AlOAlkAlPAllAlQAlmAlRAloAlSAlpAlTAlqAlUAlrAlVAltAlWAluAlXA"
	"lvAlYAlwAlZAlxAlyAlzAQ%AQsAQBAQ$AQnAQCAQ-AQ(AQDAQ;AQ)"
	"AQEAQaAQ0AQFAQbAQ1AQGAQcAQ2AQHAQdAQ3AQIAQeAQ4AQJAQfAQ5AQKAQgAQ6AQLAQhAQ7AQ"
	"MAQiAQ8AQNAQjAQ9AQOAQkAQPAQlAQQAQmAQRAQoAQSAQpAQTAQqAQUAQrAQVAQtAQWAQuAQXA"
	"QvAQYAQwAQZAQxAQyAQzAm%AmsAmBAm$AmnAmCAm-Am(AmDAm;Am)"
	"AmEAmaAm0AmFAmbAm1AmGAmcAm2AmHAmdAm3AmIAmeAm4AmJAmfAm5AmKAmgAm6AmLAmhAm7Am"
	"MAmiAm8AmNAmjAm9AmOAmkAmPAmlAmQAmmAmRAmoAmSAmpAmTAmqAmUAmrAmVAmtAmWAmuAmXA"
	"mvAmYAmwAmZAmxAmyAmzAR%ARsARBAR$ARnARCAR-AR(ARDAR;AR)"
	"AREARaAR0ARFARbAR1ARGARcAR2ARHARdAR3ARIAReAR4ARJARfAR5ARKARgAR6ARLARhAR7AR"
	"MARiAR8ARNARjAR9AROARkARPARlARQARmARRARoARSARpARTARqARUARrARVARtARWARuARXA"
	"RvARYARwARZARxARyARzAo%AosAoBAo$AonAoCAo-Ao(AoDAo;Ao)"
	"AoEAoaAo0AoFAobAo1AoGAocAo2AoHAodAo3AoIAoeAo4AoJAofAo5AoKAogAo6AoLAohAo7Ao"
	"MAoiAo8AoNAojAo9AoOAokAoPAolAoQAomAoRAooAoS";
static char small_buf[sizeof(test_str)];

int test_nfs() {
	int console_fd = sos_sys_open("90.txt", 0);
	/* test a small string from the code segment */
	int result = sos_sys_write(console_fd, test_str, strlen(test_str));
	assert(result == strlen(test_str));

	int fd2 = sos_sys_open("90.txt", FM_READ);
	result = sos_sys_read(fd2, small_buf, strlen(test_str));
	assert(result == strlen(test_str));
	//
	int fd3 = sos_sys_open("91.txt", FM_WRITE);
	result = sos_sys_write(fd3, small_buf, strlen(test_str));
	assert(result == strlen(test_str));

	assert(memcmp(test_str, small_buf, sizeof(test_str)) == 0);

	// /* test reading to a small buffer */
	// result = sos_sys_read(console_fd, small_buf, SMALL_BUF_SZ);
	// /* make sure you type in at least SMALL_BUF_SZ */
	// assert(result == SMALL_BUF_SZ);

	// /* test reading into a large on-stack buffer */
	// char stack_buf[BUF_SZ];
	// /* for this test you'll need to paste a lot of data into
	//    the console, without newlines */

	// result = sos_sys_read(console_fd, stack_buf, BUF_SZ);
	// printf("Got %u characters\n", result);
	// assert(result == BUF_SZ);

	// result = sos_sys_write(console_fd, stack_buf, BUF_SZ);
	// assert(result == BUF_SZ);

	//  this call to malloc should trigger an brk
	// char *heap_buf = malloc(BUF_SZ);
	// assert(heap_buf != NULL);

	// /* for this test you'll need to paste a lot of data into
	//    the console, without newlines */
	// result = sos_sys_read(console_fd, heap_buf, BUF_SZ);
	// assert(result == BUF_SZ);

	// result = sos_sys_write(console_fd, heap_buf, BUF_SZ);
	// assert(result == BUF_SZ);

	// /* try sleeping */
	// for (int i = 0; i < 5; i++) {
	// 	time_t prev_seconds = time(NULL);
	// 	sos_sys_usleep(1000);
	// 	time_t next_seconds = time(NULL);
	// 	assert(next_seconds > prev_seconds);
	// 	printf("Tick\n");
	// }

	return 0;
}