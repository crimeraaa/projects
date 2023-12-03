// PROJECT CREATION
//      dotnet new console --use-program-main -o ./TestProject
//      
// This creates a new Console Project template, using the old .NET 5.0 template.
// This explicitly has a Program class and a Main function.
namespace TestProject;

//  https://learn.microsoft.com/en-us/training/modules/install-configure-visual-studio-code/7-exercise-create-build-run-app
// 
// TO GET INTELLISENSE
//      1. `cd` to path of `projects.sln` (your workspace name may be different!)
//      2. Run:     `dotnet sln add <path-to-project>`
//         e.g:     `dotnet sln add ./csharp/TestProject`
// 
//      1. Run:     `dotnet sln <path-to-sln> add <path-to-project>
//         e.g:     `dotnet sln ../../projects.sln add ./
class Program {
    static void Main(string[] args) {
        Console.WriteLine("Hi mom!\n");
    }
}
