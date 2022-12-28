#!/bin/sh

SLEEPINT=4

export NEXRAN_XAPP=`kubectl get svc -n ricxapp --field-selector metadata.name=service-ricxapp-nexran-rmr -o jsonpath='{.items[0].spec.clusterIP}'`

echo NEXRAN_XAPP=$NEXRAN_XAPP ; echo

echo Listing NodeBs: ; echo
curl -i -X GET http://${NEXRAN_XAPP}:8000/v1/nodebs ; echo ; echo
echo Listing Slices: ; echo
curl -i -X GET http://${NEXRAN_XAPP}:8000/v1/slices ; echo ; echo
echo Listing Ues: ; echo
curl -i -X GET http://${NEXRAN_XAPP}:8000/v1/ues ; echo ; echo

sleep $SLEEPINT

echo "Creating NodeB (id=1):" ; echo
curl -i -X POST -H "Content-type: application/json" -d '{"type":"eNB","id":411,"mcc":"001","mnc":"01"}' http://${NEXRAN_XAPP}:8000/v1/nodebs ; echo ; echo
echo Listing NodeBs: ; echo
curl -i -X GET http://${NEXRAN_XAPP}:8000/v1/nodebs ; echo ; echo

sleep $SLEEPINT

echo "Creating Slice (name=fast)": ; echo
curl -i -X POST -H "Content-type: application/json" -d '{"name":"fast","allocation_policy":{"type":"proportional","share":1024}}' http://${NEXRAN_XAPP}:8000/v1/slices ; echo ; echo
echo Listing Slices: ; echo
curl -i -X GET http://${NEXRAN_XAPP}:8000/v1/slices ; echo ; echo

sleep $SLEEPINT

echo "Creating Slice (name=slow)": ; echo
curl -i -X POST -H "Content-type: application/json" -d '{"name":"slow","allocation_policy":{"type":"proportional","share":256}}' http://${NEXRAN_XAPP}:8000/v1/slices ; echo ; echo
echo Listing Slices: ; echo
curl -i -X GET http://${NEXRAN_XAPP}:8000/v1/slices ; echo ; echo

sleep $SLEEPINT

echo "Binding Slice to NodeB (name=fast):" ; echo
curl -i -X POST http://${NEXRAN_XAPP}:8000/v1/nodebs/enB_macro_001_001_0019b0/slices/fast ; echo ; echo
echo "Getting NodeB (name=enB_macro_001_001_0019b0):" ; echo
curl -i -X GET http://${NEXRAN_XAPP}:8000/v1/nodebs/enB_macro_001_001_0019b0 ; echo ; echo

sleep $SLEEPINT

echo "Binding Slice to NodeB (name=slow):" ; echo
curl -i -X POST http://${NEXRAN_XAPP}:8000/v1/nodebs/enB_macro_001_001_0019b0/slices/slow ; echo ; echo
echo "Getting NodeB (name=enB_macro_001_001_0019b0):" ; echo
curl -i -X GET http://${NEXRAN_XAPP}:8000/v1/nodebs/enB_macro_001_001_0019b0 ; echo ; echo

sleep $SLEEPINT

echo "Creating Ue (ue=001010123456789)" ; echo
curl -i -X POST -H "Content-type: application/json" -d '{"imsi":"001010123456789"}' http://${NEXRAN_XAPP}:8000/v1/ues ; echo ; echo
echo Listing Ues: ; echo
curl -i -X GET http://${NEXRAN_XAPP}:8000/v1/ues ; echo ; echo

sleep $SLEEPINT

echo "Binding Ue to Slice fast (imsi=001010123456789):" ; echo
curl -i -X POST http://${NEXRAN_XAPP}:8000/v1/slices/fast/ues/001010123456789 ; echo ; echo
echo "Getting Slice (name=fast):" ; echo
curl -i -X GET http://${NEXRAN_XAPP}:8000/v1/slices/fast ; echo ; echo

#echo "Creating Ue (ue=001010123456788)" ; echo
#curl -i -X POST -H "Content-type: application/json" -d '{"imsi":"001010123456788"}' http://${NEXRAN_XAPP}:8000/v1/ues ; echo ; echo
#echo Listing Ues: ; echo
#curl -i -X GET http://${NEXRAN_XAPP}:8000/v1/ues ; echo ; echo

#echo "Binding Ue (imsi=001010123456788):" ; echo
#curl -i -X POST http://${NEXRAN_XAPP}:8000/v1/slices/slow/ues/001010123456788 ; echo ; echo
#echo "Getting Slice (name=slow):" ; echo
#curl -i -X GET http://${NEXRAN_XAPP}:8000/v1/slices/slow ; echo ; echo

#sleep $SLEEPINT

#echo "Inverting priority of fast and slow slices:" ; echo

#curl -i -X PUT -H "Content-type: application/json" -d '{"allocation_policy":{"type":"proportional","share":1024}}' http://${NEXRAN_XAPP}:8000/v1/slices/slow ; echo ; echo ;

#sleep $SLEEPINT

#curl -i -X PUT -H "Content-type: application/json" -d '{"allocation_policy":{"type":"proportional","share":256}}' http://${NEXRAN_XAPP}:8000/v1/slices/fast ; echo ; echo



curl -i -X PUT -H "Content-type: application/json" -d '{"allocation_policy":{"type":"proportional","share":1024}}' http://${NEXRAN_XAPP}:8000/v1/slices/slow ; echo ; echo ;
curl -i -X PUT -H "Content-type: application/json" -d '{"allocation_policy":{"type":"proportional","share":256}}' http://${NEXRAN_XAPP}:8000/v1/slices/fast ; echo ; echo

curl -i -X PUT -H "Content-type: application/json" -d '{"allocation_policy":{"type":"proportional","share":1024}}' http://${NEXRAN_XAPP}:8000/v1/slices/fast ; echo ; echo
