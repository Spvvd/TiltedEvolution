#include <BSAnimationGraphManager.h>
#include <Games/ActorExtension.h>
#include <ModCompat/BehaviorVarSig.h>
#include <Structs/AnimationGraphDescriptorManager.h>

#include <algorithm>
#include <fstream>
#include <iostream>


BehaviorVarSig* BehaviorVarSig::single = nullptr;

BehaviorVarSig* BehaviorVarSig::Get()
{
    if (!BehaviorVarSig::single)
        BehaviorVarSig::single = new BehaviorVarSig();
    return BehaviorVarSig::single;
}

void removeWhiteSpace(std::string& aString)
{
    // TODO: FIX THIS GODDAMN THING
    // aString.erase(std::remove(aString.begin(), aString.end(), std::isspace), aString.end());
}

std::string commaSeperatedString(std::set<std::string> stringSet)
{
    std::ostringstream oss;
    std::copy(stringSet.begin(), stringSet.end(), std::ostream_iterator<std::string>(oss, ","));
    return oss.str();
}

bool isDirExist(std::string aPath)
{
    return std::filesystem::is_directory(aPath);
}

void BehaviorVarSig::initialize()
{
    const std::string PATH = TiltedPhoques::GetPath().string() + "/behaviors";
    if (!isDirExist(PATH))
        return;
    std::vector<std::string> dirs = loadDirs(PATH);
    for (auto item : dirs)
    {
        Sig* sig = loadSigFromDir(item);
        if (sig)
        {
            sigPool.push_back(*sig);
        }
        else
        {
            Add* add = loadAddFromDir(item);
            if (!add)
                continue;
            addPool.push_back(*add);
        }
    }
}

void BehaviorVarSig::patch(BSAnimationGraphManager* apManager, Actor* apActor)
{ /////////////////////////////////////////////////////////////////////////
    // check with animation graph holder
    ////////////////////////////////////////////////////////////////////////
    uint32_t hexFormID = apActor->formID;
    auto pExtendedActor = apActor->GetExtension();
    const AnimationGraphDescriptor* pGraph =
        AnimationGraphDescriptorManager::Get().GetDescriptor(pExtendedActor->GraphDescriptorHash);

    ////////////////////////////////////////////////////////////////////////
    // Already created
    ////////////////////////////////////////////////////////////////////////
    if (pGraph || failedSig.find(pExtendedActor->GraphDescriptorHash) != failedSig.end())
    {
        spdlog::warn("graph already created or failed before for {}!", hexFormID);
        return;
    }

    spdlog::info("actor with formID {:x} with hash of {} has modded behavior", hexFormID, pExtendedActor->GraphDescriptorHash);

    // Get animation variables
    auto dumpVar = apManager->DumpAnimationVariables(false);
    
    // Reverse the map
    std::unordered_map<std::string, uint32_t> reverseMap;
    for (auto item : dumpVar)
    {
        reverseMap.insert({(std::string)item.second, item.first});
    }

    // Output known animation variables
    bool bSTRMaster = false;
    for (auto pair : dumpVar)
    {
        if (pair.second == "bSTRMaster")
        {
            bSTRMaster = true;
        }
    }
    spdlog::info("known behavior variables: {} bSTRMaster: {}", dumpVar.size(), bSTRMaster);

    // Do the signature
    for (auto sig : sigPool)
    {
        spdlog::info("sig {} length {}", sig.sigName, sig.sigStrings.size());

        bool isSig = true;
        for (std::string sigVar : sig.sigStrings)
        {
            if (reverseMap.find(sigVar) != reverseMap.end())
            {
                spdlog::info("{} found", sig.sigName);
                continue;
            }
            else
            {
                isSig = false;
                break;
            }
        }
        for (std::string negSigVar : sig.negSigStrings)
        {
            if (reverseMap.find(negSigVar) != reverseMap.end())
            {
                spdlog::error("negSig {} not found in reversemap", negSigVar);
                isSig = false;
                break;
            }
        }

        // Signature not found
        if (!isSig)
        {
            spdlog::error("sig not found as {}", sig.sigName);
            continue;
        }

        spdlog::info("sig found as {}", sig.sigName);

        ////////////////////////////////////////////////////////////////////////
        // calculate hash
        ////////////////////////////////////////////////////////////////////////
        uint64_t mHash = apManager->GetDescriptorKey();

        spdlog::info("sig {} has a animGraph hash of {}", sig.sigName, mHash);

        ////////////////////////////////////////////////////////////////////////
        // prepare the synced var
        ////////////////////////////////////////////////////////////////////////
        TiltedPhoques::Vector<uint32_t> boolVar;
        TiltedPhoques::Vector<uint32_t> floatVar;
        TiltedPhoques::Vector<uint32_t> intVar;

        ////////////////////////////////////////////////////////////////////////
        // fill the vector
        ////////////////////////////////////////////////////////////////////////

        spdlog::info("prepraring var to sync");

        //spdlog::info("boolean variable");

        for (std::string var : sig.syncBooleanVar)
        {
            if (reverseMap.find(var) != reverseMap.end())
            {

                //spdlog::info("{}:{}", reverseMap[var], var);

                boolVar.push_back(reverseMap[var]);
            }
        }

        //spdlog::info("float variable");

        for (std::string var : sig.syncFloatVar)
        {
            if (reverseMap.find(var) != reverseMap.end())
            {

                //spdlog::info("{}:{}", reverseMap[var], var);

                floatVar.push_back(reverseMap[var]);
            }
        }

        //spdlog::info("integer variable");

        for (std::string var : sig.syncIntegerVar)
        {
            if (reverseMap.find(var) != reverseMap.end())
            {

                //spdlog::info("{}:{}", reverseMap[var], var);

                intVar.push_back(reverseMap[var]);
            }
        }

        ////////////////////////////////////////////////////////////////////////
        // Very hacky and shouldnt be allowed
        // This is a breach in the dev code and will not be merged
        ////////////////////////////////////////////////////////////////////////

        spdlog::info("building animgraph var for {0:x}", hexFormID);

        auto animGrapDescriptor = new AnimationGraphDescriptor({0}, {0}, {0});
        animGrapDescriptor->BooleanLookUpTable = boolVar;
        animGrapDescriptor->FloatLookupTable = floatVar;
        animGrapDescriptor->IntegerLookupTable = intVar;

        ////////////////////////////////////////////////////////////////////////
        // add the new graph to the var graph
        ////////////////////////////////////////////////////////////////////////
        new AnimationGraphDescriptorManager::Builder(AnimationGraphDescriptorManager::Get(), mHash,
                                                     *animGrapDescriptor);

        ////////////////////////////////////////////////////////////////////////
        // change the actor hash? is this even necessary?
        ////////////////////////////////////////////////////////////////////////
        pExtendedActor->GraphDescriptorHash = mHash;

        spdlog::info("new hash set: {}", mHash);

        ////////////////////////////////////////////////////////////////////////
        // handle hard coded case
        ////////////////////////////////////////////////////////////////////////
        /*if (sig.sigName == "master")
        {
            humanoidSig = {mHash, sig};
            new AnimationGraphDescriptorManager::Builder(AnimationGraphDescriptorManager::Get(), 17585368238253125375,
                                                         *animGrapDescriptor);
        }
         else if (sig.sigName == "werewolf")
            werewolfSig = {mHash, sig};
        else if (sig.sigName == "vampire_lord")
            vampireSig = {mHash, sig};
        */
        ////////////////////////////////////////////////////////////////////////
        // take a break buddy
        ////////////////////////////////////////////////////////////////////////
        return;
    }

    // sig failed

    spdlog::error("sig for actor {:x} failed with hash {}", hexFormID, pExtendedActor->GraphDescriptorHash);

    failedSig[pExtendedActor->GraphDescriptorHash] = true;
}

std::vector<std::string> BehaviorVarSig::loadDirs(const std::string& acPATH)
{
    std::vector<std::string> result;
    for (auto& p : std::filesystem::directory_iterator(acPATH))
        if (p.is_directory())
            result.push_back(p.path().string());
    return result;
}

BehaviorVarSig::Sig* BehaviorVarSig::loadSigFromDir(std::string aDir)
{

    spdlog::info("creating sig");

    std::string nameVarFileDir;
    std::string sigFileDir;
    std::vector<std::string> floatVarFileDir;
    std::vector<std::string> intVarFileDir;
    std::vector<std::string> boolVarFileDir;
    bool isHash = false;

    ////////////////////////////////////////////////////////////////////////
    // Enumerate all files in this directory
    ////////////////////////////////////////////////////////////////////////

    for (auto& p : std::filesystem::directory_iterator(aDir))
    {
        std::string path = p.path().string();
        std::string base_filename = path.substr(path.find_last_of("/\\") + 1);

        spdlog::info("base_path: {}", base_filename);

        if (base_filename.find("__name.txt") != std::string::npos)
        {
            nameVarFileDir = path;

            spdlog::info("name file: {}", nameVarFileDir);
        }
        else if (base_filename.find("__sig.txt") != std::string::npos)
        {
            sigFileDir = path;

            spdlog::info("sig file: {}", path);
        }
        else if (base_filename.find("__float.txt") != std::string::npos)
        {
            floatVarFileDir.push_back(path);

            spdlog::info("float file: {}", path);
        }
        else if (base_filename.find("__int.txt") != std::string::npos)
        {
            intVarFileDir.push_back(path);

            spdlog::info("int file: {}", path);
        }
        else if (base_filename.find("__bool.txt") != std::string::npos)
        {
            boolVarFileDir.push_back(path);

            spdlog::info("bool file: {}", path);
        }
    }

    ////////////////////////////////////////////////////////////////////////
    // sanity check
    ////////////////////////////////////////////////////////////////////////
    if (nameVarFileDir == "" || sigFileDir == "")
    {
        return nullptr;
    }

    ////////////////////////////////////////////////////////////////////////
    // read the files
    ////////////////////////////////////////////////////////////////////////
    std::string name = "";
    std::vector<std::string> sig;
    std::vector<std::string> negSig;
    std::set<std::string> floatVar;
    std::set<std::string> intVar;
    std::set<std::string> boolVar;

    // read name var
    std::string tempString;
    std::ifstream file(nameVarFileDir);
    getline(file, tempString);
    name = tempString;
    removeWhiteSpace(name);
    file.close();
    if (name == "")
        return nullptr;

    // read sig var

    spdlog::info("creating sig for {}", name);

    std::ifstream file1(sigFileDir);
    while (std::getline(file1, tempString))
    {
        removeWhiteSpace(tempString);
        if (tempString.find("~") != std::string::npos)
        {
            negSig.push_back(tempString.substr(tempString.find("~") + 1));

            spdlog::info("~{}:{}", name, tempString.substr(tempString.find("~") + 1));
        }
        else
        {
            sig.push_back(tempString);

            spdlog::info("{}:{}", name, tempString);
        }
    }
    file1.close();
    if (sig.size() < 1)
    {
        return nullptr;
    }

    spdlog::info("reading float var", name, tempString);

    // read float var
    for (auto item : floatVarFileDir)
    {
        std::ifstream file2(item);
        while (std::getline(file2, tempString))
        {
            removeWhiteSpace(tempString);
            floatVar.insert(tempString);

            spdlog::info(tempString);
        }
        file2.close();
    }

    spdlog::info("reading int var", name, tempString);

    // read int var
    for (auto item : intVarFileDir)
    {
        std::ifstream file3(item);
        while (std::getline(file3, tempString))
        {
            removeWhiteSpace(tempString);
            intVar.insert(tempString);

            spdlog::info(tempString);
        }
        file3.close();
    }

    spdlog::info("reading bool var", name, tempString);

    // read bool var
    for (auto item : boolVarFileDir)
    {
        std::ifstream file4(item);
        while (std::getline(file4, tempString))
        {
            removeWhiteSpace(tempString);
            boolVar.insert(tempString);

            spdlog::info(tempString);
        }
        file4.close();
    }

    // convert set to vector
    std::vector<std::string> floatVector;
    std::vector<std::string> intVector;
    std::vector<std::string> boolVector;

    for (auto item : floatVar)
    {
        floatVector.push_back(item);
    }

    for (auto item : intVar)
    {
        intVector.push_back(item);
    }

    for (auto item : boolVar)
    {
        boolVector.push_back(item);
    }

    // create the sig
    Sig* result = new Sig();

    result->sigName = name;
    result->sigStrings = sig;
    result->negSigStrings = negSig;
    result->syncBooleanVar = boolVector;
    result->syncFloatVar = floatVector;
    result->syncIntegerVar = intVector;

    return result;
}

BehaviorVarSig::Add* BehaviorVarSig::loadAddFromDir(std::string aDir)
{

    spdlog::info("creating hash patch");

    std::string mHashFileDir;
    std::vector<std::string> floatVarFileDir;
    std::vector<std::string> intVarFileDir;
    std::vector<std::string> boolVarFileDir;
    bool isHash = false;

    ////////////////////////////////////////////////////////////////////////
    // Enumerate all files in this directory
    ////////////////////////////////////////////////////////////////////////

    for (auto& p : std::filesystem::directory_iterator(aDir))
    {
        std::string path = p.path().string();
        std::string base_filename = path.substr(path.find_last_of("/\\") + 1);

        spdlog::info("base_path: {}", base_filename);

        if (base_filename.find("__hash.txt") != std::string::npos)
        {
            mHashFileDir = path;

            spdlog::info("hash file: {}", mHashFileDir);
        }
        else if (base_filename.find("__float.txt") != std::string::npos)
        {
            floatVarFileDir.push_back(path);

            spdlog::info("float file: {}", path);
        }
        else if (base_filename.find("__int.txt") != std::string::npos)
        {
            intVarFileDir.push_back(path);

            spdlog::info("int file: {}", path);
        }
        else if (base_filename.find("__bool.txt") != std::string::npos)
        {
            boolVarFileDir.push_back(path);

            spdlog::info("bool file: {}", path);
        }
    }

    ////////////////////////////////////////////////////////////////////////
    // sanity check
    ///////////////////////////////////////////////////////////////////////

    if (mHashFileDir == "")
        return nullptr;

    ////////////////////////////////////////////////////////////////////////
    // try getting the hash
    ////////////////////////////////////////////////////////////////////////
    std::uint64_t hash;
    std::set<uint32_t> floatVar;
    std::set<uint32_t> intVar;
    std::set<uint32_t> boolVar;

    // read name var
    std::string tempString;
    std::ifstream file(mHashFileDir);
    getline(file, tempString);
    removeWhiteSpace(tempString);
    file.close();
    if (tempString == "")
        return nullptr;

    try
    {
        std::stringstream ss(tempString);
        if ((ss >> hash).fail() || !(ss >> std::ws).eof())
        {
            throw std::bad_cast();
        }

        spdlog::info("hash found: {}", hash);
    }
    catch (const std::bad_cast& e)
    {

        spdlog::info("bad hash inputed: {}", tempString);

        return nullptr;
    }

    ////////////////////////////////////////////////////////////////////////
    // read the files
    ////////////////////////////////////////////////////////////////////////

    spdlog::info("reading float var");

    // read float var
    for (auto item : floatVarFileDir)
    {
        std::ifstream file2(item);
        while (std::getline(file2, tempString))
        {
            removeWhiteSpace(tempString);
            try
            {
                uint32_t temp;
                std::stringstream ss(tempString);
                if ((ss >> temp).fail() || !(ss >> std::ws).eof())
                {
                    throw std::bad_cast();
                }
                floatVar.insert(temp);

                spdlog::info(tempString);
            }
            catch (const std::bad_cast& e)
            {
                continue;
            }
        }
        file2.close();
    }

    spdlog::info("reading int var");

    // read int var
    for (auto item : intVarFileDir)
    {
        std::ifstream file3(item);
        while (std::getline(file3, tempString))
        {
            removeWhiteSpace(tempString);
            try
            {
                uint32_t temp;
                std::stringstream ss(tempString);
                if ((ss >> temp).fail() || !(ss >> std::ws).eof())
                {
                    throw std::bad_cast();
                }
                intVar.insert(temp);

                spdlog::info(tempString);
            }
            catch (const std::bad_cast& e)
            {
                continue;
            }
        }
        file3.close();
    }

    spdlog::info("reading bool var");

    // read bool var
    for (auto item : boolVarFileDir)
    {
        std::ifstream file4(item);
        while (std::getline(file4, tempString))
        {
            removeWhiteSpace(tempString);
            try
            {
                uint32_t temp;
                std::stringstream ss(tempString);
                if ((ss >> temp).fail() || !(ss >> std::ws).eof())
                {
                    throw std::bad_cast();
                }
                boolVar.insert(temp);

                spdlog::info(tempString);
            }
            catch (const std::bad_cast& e)
            {
                continue;
            }
        }
        file4.close();
    }

    // create the add
    Add* result = new Add;
    result->mHash = hash;
    result->syncBooleanVar.assign(boolVar.begin(), boolVar.end());
    result->syncFloatVar.assign(floatVar.begin(), floatVar.end());
    result->syncIntegerVar.assign(intVar.begin(), intVar.end());

    return result;
}

void BehaviorVarSig::patchAdd(BehaviorVarSig::Add& aAdd)
{

    spdlog::info("patching hash of {}", aAdd.mHash);

    const AnimationGraphDescriptor* pGraph = AnimationGraphDescriptorManager::Get().GetDescriptor(aAdd.mHash);
    if (!pGraph)
    {

        spdlog::info("patching hash of {} not found", aAdd.mHash);

        return;
    }

    std::map<uint32_t, bool> boolVar;
    std::map<uint32_t, bool> floatVar;
    std::map<uint32_t, bool> intVar;

    for (auto item : pGraph->BooleanLookUpTable)
    {
        boolVar.insert({item, true});
    }
    for (auto item : pGraph->FloatLookupTable)
    {
        floatVar.insert({item, true});
    }
    for (auto item : pGraph->IntegerLookupTable)
    {
        intVar.insert({item, true});
    }

    spdlog::info("boolean var", aAdd.mHash);

    for (auto item : aAdd.syncBooleanVar)
    {

        spdlog::info("{}", item);

        boolVar.insert({item, true});
    }

    spdlog::info("float var", aAdd.mHash);

    for (auto item : aAdd.syncFloatVar)
    {
        floatVar.insert({item, true});

        spdlog::info("{}", item);
    }

    spdlog::info("int var", aAdd.mHash);

    for (auto item : aAdd.syncIntegerVar)
    {

        spdlog::info("{}", item);

        intVar.insert({item, true});
    }

    TiltedPhoques::Vector<uint32_t> bVar;
    TiltedPhoques::Vector<uint32_t> fVar;
    TiltedPhoques::Vector<uint32_t> iVar;

    for (auto item : boolVar)
    {
        bVar.push_back(item.first);
    }
    for (auto item : floatVar)
    {
        fVar.push_back(item.first);
    }
    for (auto item : intVar)
    {
        iVar.push_back(item.first);
    }

    auto animGrapDescriptor = new AnimationGraphDescriptor({0}, {0}, {0});
    animGrapDescriptor->BooleanLookUpTable = bVar;
    animGrapDescriptor->FloatLookupTable = fVar;
    animGrapDescriptor->IntegerLookupTable = iVar;

    AnimationGraphDescriptorManager::Get().ReRegister(aAdd.mHash, *animGrapDescriptor);
    // new AnimationGraphDescriptorManager::Builder(AnimationGraphDescriptorManager::Get(), aAdd.mHash,
    // *animGrapDescriptor);
}
